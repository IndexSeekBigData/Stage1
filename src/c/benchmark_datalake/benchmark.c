
#include "datalake.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define MAX_BOOKS 1000
#define MAX_PATH_LENGTH 4096
#define REPETITIONS 5
#define LOOKUP_REPETITIONS 100
#define RANGE_SIZE 25

#define SAMPLE_PATH "sample_data"
#define OUTPUT_PATH "datalake_benchmark\\c"
#define RESULTS_PATH "benchmark"

typedef struct {
    double mean;
    double std;
} Stats;

typedef struct {
    double mb;
    long long files;
    long long directories;
} Storage;

typedef struct {
    int ids[MAX_BOOKS];
    int count;
} IdList;

static const char *names[] = {
    "time_based",
    "book_based",
    "range_based"
};

static const char *date_value = "20261004";
static const char *hour_value = "22";

static void fail(const char *message) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(EXIT_FAILURE);
}

static void check_path(int length, size_t capacity) {
    if (length < 0 || (size_t)length >= capacity) {
        fail("Ruta demasiado larga");
    }
}

static double now_seconds(void) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart /
           (double)frequency.QuadPart;
}

static Stats calculate_stats(const double *values, int count) {
    Stats result = {0, 0};

    if (count <= 0) {
        return result;
    }

    for (int i = 0; i < count; i++) {
        result.mean += values[i];
    }

    result.mean /= count;

    if (count > 1) {
        for (int i = 0; i < count; i++) {
            double difference = values[i] - result.mean;
            result.std += difference * difference;
        }

        result.std = sqrt(result.std / (count - 1));
    }

    return result;
}

static void print_stats(
    const char *name,
    const double *values,
    int count,
    const char *unit
) {
    Stats s = calculate_stats(values, count);

    printf(
        "%-12s | %.6f %s | desviacion: %.6f %s\n",
        name,
        s.mean,
        unit,
        s.std,
        unit
    );
}

static void ensure_directory(const char *path) {
    if (!create_directory(path)) {
        fprintf(stderr, "No se pudo crear: %s\n", path);
        exit(EXIT_FAILURE);
    }
}

static void structure_path(int structure, char *path) {
    check_path(
        snprintf(
            path,
            MAX_PATH_LENGTH,
            "%s\\%s",
            OUTPUT_PATH,
            names[structure]
        ),
        MAX_PATH_LENGTH
    );
}

static int compare_books(const void *a, const void *b) {
    const Book *first = a;
    const Book *second = b;

    return (first->id > second->id) -
           (first->id < second->id);
}

static int compare_ids(const void *a, const void *b) {
    int first = *(const int *)a;
    int second = *(const int *)b;

    return (first > second) - (first < second);
}

static int load_books(Book *books) {
    WIN32_FIND_DATAA data;

    HANDLE handle = FindFirstFileA(
        SAMPLE_PATH "\\*.body.txt",
        &data
    );

    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int count = 0;

    do {
        int id;
        char extra;

        if (sscanf(data.cFileName, "%d.body.txt%c", &id, &extra) != 1) {
            continue;
        }

        if (id < 0) {
            continue;
        }

        char body[MAX_PATH_LENGTH];
        char header[MAX_PATH_LENGTH];

        check_path(
            snprintf(
                body,
                sizeof(body),
                "%s\\%d.body.txt",
                SAMPLE_PATH,
                id
            ),
            sizeof(body)
        );

        check_path(
            snprintf(
                header,
                sizeof(header),
                "%s\\%d.header.txt",
                SAMPLE_PATH,
                id
            ),
            sizeof(header)
        );

        if (!file_exists(header)) {
            continue;
        }

        if (count >= MAX_BOOKS) {
            FindClose(handle);
            fail("Demasiados libros");
        }

        books[count].id = id;
        books[count].body = _strdup(body);
        books[count].header = _strdup(header);
        books[count].date = _strdup(date_value);

        if (!books[count].body ||
            !books[count].header ||
            !books[count].date) {
            FindClose(handle);
            fail("Error reservando memoria");
        }

        count++;

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    qsort(books, count, sizeof(Book), compare_books);

    return count;
}

static int write_book(
    int structure,
    const char *root,
    const Book *book
) {
    switch (structure) {
        case 0:
            return time_based_write(root, book);
        case 1:
            return book_based_write(root, book);
        case 2:
            return range_based_write(root, book);
        default:
            return 0;
    }
}

static void write_books(
    int structure,
    const char *root,
    const Book *books,
    int start,
    int end
) {
    for (int i = start; i < end; i++) {
        if (!write_book(structure, root, &books[i])) {
            fprintf(
                stderr,
                "Error escribiendo libro %d en %s\n",
                books[i].id,
                names[structure]
            );

            exit(EXIT_FAILURE);
        }
    }
}

static void clean_structure(const char *root) {
    DWORD attributes = GetFileAttributesA(root);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    if (!delete_directory(root)) {
        fprintf(stderr, "Error eliminando: %s\n", root);
        exit(EXIT_FAILURE);
    }
}

static void clean_all(void) {
    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);
        clean_structure(root);
    }
}

static int recursive_find(
    const char *directory,
    const char *filename
) {
    char pattern[MAX_PATH_LENGTH];

    check_path(
        snprintf(
            pattern,
            sizeof(pattern),
            "%s\\*",
            directory
        ),
        sizeof(pattern)
    );

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int found = 0;

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LENGTH];

        check_path(
            snprintf(
                path,
                sizeof(path),
                "%s\\%s",
                directory,
                data.cFileName
            ),
            sizeof(path)
        );

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recursive_find(path, filename)) {
                found = 1;
                break;
            }
        } else if (strcmp(data.cFileName, filename) == 0) {
            found = 1;
            break;
        }

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    return found;
}

static int lookup(
    int structure,
    const char *root,
    const Book *book
) {
    if (structure == 0) {
        char filename[128];

        check_path(
            snprintf(
                filename,
                sizeof(filename),
                "%d.body.txt",
                book->id
            ),
            sizeof(filename)
        );

        return recursive_find(root, filename);
    }

    if (structure == 1) {
        char path[MAX_PATH_LENGTH];

        check_path(
            snprintf(
                path,
                sizeof(path),
                "%s\\%d\\%d.body.txt",
                root,
                book->id,
                book->id
            ),
            sizeof(path)
        );

        return file_exists(path);
    }

    if (structure == 2) {
        int start = (book->id / RANGE_SIZE) * RANGE_SIZE;

        char path[MAX_PATH_LENGTH];

        check_path(
            snprintf(
                path,
                sizeof(path),
                "%s\\%d-%d\\%d.body.txt",
                root,
                start,
                start + RANGE_SIZE - 1,
                book->id
            ),
            sizeof(path)
        );

        return file_exists(path);
    }

    return 0;
}

static void verify_lookup(
    int structure,
    const char *root,
    const Book *books,
    int count
) {
    for (int i = 0; i < count; i++) {
        if (!lookup(structure, root, &books[i])) {
            fprintf(
                stderr,
                "Libro no encontrado: %d\n",
                books[i].id
            );

            exit(EXIT_FAILURE);
        }
    }
}

static void collect_ids(
    const char *directory,
    const char *suffix,
    IdList *result
) {
    char pattern[MAX_PATH_LENGTH];

    check_path(
        snprintf(
            pattern,
            sizeof(pattern),
            "%s\\*",
            directory
        ),
        sizeof(pattern)
    );

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LENGTH];

        check_path(
            snprintf(
                path,
                sizeof(path),
                "%s\\%s",
                directory,
                data.cFileName
            ),
            sizeof(path)
        );

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_ids(path, suffix, result);
            continue;
        }

        size_t filename_length = strlen(data.cFileName);
        size_t suffix_length = strlen(suffix);

        if (filename_length <= suffix_length) {
            continue;
        }

        if (strcmp(
                data.cFileName + filename_length - suffix_length,
                suffix
            ) != 0) {
            continue;
        }

        int id;
        char extra;

        if (sscanf(data.cFileName, "%d.%c", &id, &extra) != 2) {
            continue;
        }

        if (id < 0) {
            continue;
        }

        if (result->count >= MAX_BOOKS) {
            FindClose(handle);
            fail("Demasiados identificadores");
        }

        result->ids[result->count++] = id;

    } while (FindNextFileA(handle, &data));

    FindClose(handle);
}

static Storage storage_recursive(const char *directory) {
    Storage result = {0, 0, 0};

    char pattern[MAX_PATH_LENGTH];

    check_path(
        snprintf(
            pattern,
            sizeof(pattern),
            "%s\\*",
            directory
        ),
        sizeof(pattern)
    );

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE) {
        return result;
    }

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        char path[MAX_PATH_LENGTH];

        check_path(
            snprintf(
                path,
                sizeof(path),
                "%s\\%s",
                directory,
                data.cFileName
            ),
            sizeof(path)
        );

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            result.directories++;

            Storage nested = storage_recursive(path);

            result.files += nested.files;
            result.directories += nested.directories;
            result.mb += nested.mb;
        } else {
            ULARGE_INTEGER size;

            size.HighPart = data.nFileSizeHigh;
            size.LowPart = data.nFileSizeLow;

            result.files++;
            result.mb += (double)size.QuadPart /
                         (1024.0 * 1024.0);
        }

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    return result;
}

static void benchmark_write(const Book *books, int count) {
    printf("\n========== WRITE / THROUGHPUT ==========\n");

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);

        double times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            clean_structure(root);

            double start = now_seconds();
            write_books(structure, root, books, 0, count);
            times[r] = now_seconds() - start;
        }

        Stats s = calculate_stats(times, REPETITIONS);

        print_stats(names[structure], times, REPETITIONS, "s");

        printf(
            "               %.2f libros/s\n",
            count / s.mean
        );
    }
}

static void benchmark_lookup(const Book *books, int count) {
    printf("\n========== LOOKUP ==========\n");

    clean_all();

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);
        write_books(structure, root, books, 0, count);
    }

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);

        double times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            double start = now_seconds();

            for (int i = 0; i < LOOKUP_REPETITIONS; i++) {
                verify_lookup(structure, root, books, count);
            }

            times[r] = (now_seconds() - start) * 1000.0 /
                       (LOOKUP_REPETITIONS * count);
        }

        print_stats(
            names[structure],
            times,
            REPETITIONS,
            "ms/busqueda"
        );
    }
}

static void benchmark_incremental(const Book *books, int count) {
    if (count < 10) {
        return;
    }

    int initial = count - 10;

    printf("\n========== INCREMENTAL INSERTION ==========\n");
    printf("Libros iniciales: %d\n", initial);
    printf("Libros anadidos: 10\n");
    printf("IDs anadidos: ");

    for (int i = initial; i < count; i++) {
        printf(
            "%d%s",
            books[i].id,
            i == count - 1 ? "\n" : ", "
        );
    }

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);

        double times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            clean_structure(root);
            write_books(structure, root, books, 0, initial);

            double start = now_seconds();
            write_books(structure, root, books, initial, count);
            times[r] = now_seconds() - start;
        }

        print_stats(
            names[structure],
            times,
            REPETITIONS,
            "s"
        );
    }
}

static void benchmark_discovery(const Book *books, int count) {
    if (count < 10) {
        return;
    }

    int initial = count - 10;

    printf("\n========== PENDING BOOK DISCOVERY ==========\n");
    printf("Libros descargados: %d\n", count);
    printf("Libros indexados: %d\n", initial);
    printf("IDs pendientes esperados: ");

    for (int i = initial; i < count; i++) {
        printf(
            "%d%s",
            books[i].id,
            i == count - 1 ? "\n" : ", "
        );
    }

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);

        clean_structure(root);
        write_books(structure, root, books, 0, count);

        double times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            IdList downloaded = {0};

            double start = now_seconds();

            collect_ids(root, ".body.txt", &downloaded);

            int pending[MAX_BOOKS];
            int pending_count = 0;

            for (int i = 0; i < downloaded.count; i++) {
                int indexed = 0;

                for (int j = 0; j < initial; j++) {
                    if (downloaded.ids[i] == books[j].id) {
                        indexed = 1;
                        break;
                    }
                }

                if (!indexed) {
                    pending[pending_count++] = downloaded.ids[i];
                }
            }

            qsort(
                pending,
                pending_count,
                sizeof(int),
                compare_ids
            );

            times[r] = now_seconds() - start;

            if (pending_count != 10) {
                fail("Numero incorrecto de libros pendientes");
            }

            for (int i = 0; i < 10; i++) {
                if (pending[i] != books[initial + i].id) {
                    fail("IDs pendientes incorrectos");
                }
            }
        }

        print_stats(
            names[structure],
            times,
            REPETITIONS,
            "s"
        );
    }
}

static void benchmark_storage(const Book *books, int count) {
    printf("\n========== STORAGE ==========\n");

    clean_all();

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        structure_path(structure, root);

        write_books(structure, root, books, 0, count);

        Storage s = storage_recursive(root);

        printf(
            "%-12s | %.2f MB | %lld archivos | %lld directorios\n",
            names[structure],
            s.mb,
            s.files,
            s.directories
        );
    }
}

static void build_body_path(
    int structure,
    const char *root,
    const Book *book,
    char *path
) {
    if (structure == 0) {
        check_path(
            snprintf(
                path,
                MAX_PATH_LENGTH,
                "%s\\%s\\%s\\%d.body.txt",
                root,
                book->date,
                hour_value,
                book->id
            ),
            MAX_PATH_LENGTH
        );
    } else if (structure == 1) {
        check_path(
            snprintf(
                path,
                MAX_PATH_LENGTH,
                "%s\\%d\\%d.body.txt",
                root,
                book->id,
                book->id
            ),
            MAX_PATH_LENGTH
        );
    } else {
        int start = (book->id / RANGE_SIZE) * RANGE_SIZE;

        check_path(
            snprintf(
                path,
                MAX_PATH_LENGTH,
                "%s\\%d-%d\\%d.body.txt",
                root,
                start,
                start + RANGE_SIZE - 1,
                book->id
            ),
            MAX_PATH_LENGTH
        );
    }
}

static void benchmark_recovery(const Book *books, int count) {
    if (count == 0) {
        return;
    }

    printf("\n========== RECOVERY AFTER INTERRUPTION ==========\n");
    printf("Libro interrumpido: %d\n", books[0].id);

    for (int structure = 0; structure < 3; structure++) {
        char root[MAX_PATH_LENGTH];
        char body_path[MAX_PATH_LENGTH];
        char header_path[MAX_PATH_LENGTH];

        structure_path(structure, root);

        clean_structure(root);
        write_books(structure, root, books, 0, count);

        build_body_path(
            structure,
            root,
            &books[0],
            body_path
        );

        if (!DeleteFileA(body_path)) {
            fail("No se pudo simular la interrupcion");
        }

        size_t length = strlen(body_path);
        size_t suffix_length = strlen(".body.txt");

        if (length <= suffix_length) {
            fail("Ruta de body incorrecta");
        }

        check_path(
            snprintf(
                header_path,
                sizeof(header_path),
                "%.*s.header.txt",
                (int)(length - suffix_length),
                body_path
            ),
            sizeof(header_path)
        );

        if (!file_exists(header_path) || file_exists(body_path)) {
            fail("Estado de interrupcion incorrecto");
        }

        double start = now_seconds();

        write_book(structure, root, &books[0]);

        double elapsed = now_seconds() - start;

        IdList bodies = {0};
        IdList headers = {0};

        collect_ids(root, ".body.txt", &bodies);
        collect_ids(root, ".header.txt", &headers);

        qsort(
            bodies.ids,
            bodies.count,
            sizeof(int),
            compare_ids
        );

        qsort(
            headers.ids,
            headers.count,
            sizeof(int),
            compare_ids
        );

        int valid = bodies.count == count &&
                    headers.count == count;

        if (valid) {
            for (int i = 0; i < count; i++) {
                if (bodies.ids[i] != books[i].id ||
                    headers.ids[i] != books[i].id) {
                    valid = 0;
                    break;
                }
            }
        }

        printf(
            "%-12s | %.6f s | headers: %d | bodies: %d | pares: %d | %s\n",
            names[structure],
            elapsed,
            headers.count,
            bodies.count,
            valid ? count : 0,
            valid ? "OK" : "ERROR"
        );

        if (!valid) {
            fail("Recuperacion incorrecta");
        }
    }
}

static void benchmark_scalability(const Book *books, int count) {
    printf("\n========== SCALABILITY ==========\n");

    int sizes[] = {10, 25, 50, 100};

    ensure_directory(RESULTS_PATH);

    FILE *csv = fopen(
        RESULTS_PATH "\\c_datalake_scalability.csv",
        "w"
    );

    if (!csv) {
        fail("No se pudo crear el CSV");
    }

    fprintf(
        csv,
        "structure,books,write_mean_s,write_std_s,"
        "throughput_books_s,lookup_mean_ms,lookup_std_ms,"
        "storage_mb,files,directories\n"
    );

    for (int k = 0; k < 4; k++) {
        int size = sizes[k];

        if (size > count) {
            continue;
        }

        printf("\n========== CORPUS: %d BOOKS ==========\n", size);

        for (int structure = 0; structure < 3; structure++) {
            char root[MAX_PATH_LENGTH];
            structure_path(structure, root);

            clean_structure(root);
            write_books(structure, root, books, 0, size);
            clean_structure(root);

            double write_times[REPETITIONS];
            double lookup_times[REPETITIONS];

            for (int r = 0; r < REPETITIONS; r++) {
                clean_structure(root);

                double start = now_seconds();
                write_books(structure, root, books, 0, size);
                write_times[r] = now_seconds() - start;
            }

            verify_lookup(structure, root, books, size);

            for (int r = 0; r < REPETITIONS; r++) {
                double start = now_seconds();

                for (int i = 0; i < LOOKUP_REPETITIONS; i++) {
                    verify_lookup(structure, root, books, size);
                }

                lookup_times[r] =
                    (now_seconds() - start) * 1000.0 /
                    (LOOKUP_REPETITIONS * size);
            }

            Stats write = calculate_stats(
                write_times,
                REPETITIONS
            );

            Stats lookup_stats = calculate_stats(
                lookup_times,
                REPETITIONS
            );

            Storage disk = storage_recursive(root);

            printf(
                "%-12s | escritura: %.6f s | busqueda: %.6f ms | %.2f MB\n",
                names[structure],
                write.mean,
                lookup_stats.mean,
                disk.mb
            );

            fprintf(
                csv,
                "%s,%d,%.6f,%.6f,%.2f,%.6f,%.6f,%.4f,%lld,%lld\n",
                names[structure],
                size,
                write.mean,
                write.std,
                size / write.mean,
                lookup_stats.mean,
                lookup_stats.std,
                disk.mb,
                disk.files,
                disk.directories
            );
        }
    }

    fclose(csv);
}

int main(void) {
    Book books[MAX_BOOKS] = {0};

    int count = load_books(books);

    printf("========================================\n");
    printf("        DATALAKE BENCHMARK - C\n");
    printf("========================================\n");
    printf("Libros cargados: %d\n", count);
    printf("Repeticiones: %d\n", REPETITIONS);
    printf("Sample data: %s\n", SAMPLE_PATH);
    printf("Benchmark path: %s\n", OUTPUT_PATH);

    if (count < 10) {
        fail("No hay suficientes libros en sample_data");
    }

    ensure_directory("datalake_benchmark");
    ensure_directory(OUTPUT_PATH);

    benchmark_write(books, count);
    benchmark_lookup(books, count);
    benchmark_incremental(books, count);
    benchmark_discovery(books, count);
    benchmark_storage(books, count);
    benchmark_recovery(books, count);
    benchmark_scalability(books, count);

    for (int i = 0; i < count; i++) {
        free(books[i].body);
        free(books[i].header);
        free(books[i].date);
    }

    printf("\n========================================\n");
    printf("          BENCHMARK COMPLETADO\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
