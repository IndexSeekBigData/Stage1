
#include "book_loader.h"
#include "inverted_index.h"
#include "monolithic_json.h"
#include "term_files.h"
#include "hierarchical.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPETITIONS 3
#define QUERY_REPETITIONS 100
#define QUERY_COUNT 9
#define STRUCTURE_COUNT 3
#define MAX_PATH_LENGTH 4096

#define SAMPLE_PATH "sample_data"
#define INDEX_ROOT "index_benchmark\\c"
#define RESULTS_PATH "benchmark\\c_index_incremental.csv"

typedef struct {
    double mean;
    double std;
} Stats;

typedef struct {
    unsigned long long bytes;
    unsigned long long files;
    unsigned long long directories;
} Storage;

typedef struct {
    const char *name;
    int (*save)(const InvertedIndex *, const char *);
    int (*update)(const InvertedIndex *, const char *);
    PostingList *(*search)(const char *, const char *);
} Structure;

static const char *queries[QUERY_COUNT] = {
    "the",
    "and",
    "love",
    "man",
    "woman",
    "time",
    "house",
    "book",
    "zzzznotfound"
};

static const Structure structures[STRUCTURE_COUNT] = {
    {
        "monolithic",
        monolithic_save,
        monolithic_update,
        monolithic_search
    },
    {
        "term_files",
        term_files_save,
        term_files_update,
        term_files_search
    },
    {
        "hierarchical",
        hierarchical_save,
        hierarchical_update,
        hierarchical_search
    }
};

static void fail(const char *message) {
    fprintf(stderr, "\nERROR: %s\n", message);
    exit(EXIT_FAILURE);
}

static double now_seconds(void) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter)) {
        fail("No se pudo consultar el reloj");
    }

    return (double)counter.QuadPart /
           (double)frequency.QuadPart;
}

static Stats summarize(const double *times, int count) {
    Stats result = {0.0, 0.0};

    for (int i = 0; i < count; i++) {
        result.mean += times[i];
    }

    result.mean /= count;

    if (count > 1) {
        for (int i = 0; i < count; i++) {
            double difference = times[i] - result.mean;
            result.std += difference * difference;
        }

        result.std = sqrt(result.std / (count - 1));
    }

    return result;
}

static void structure_path(
    int structure,
    char *path,
    size_t capacity
) {
    int length = snprintf(
        path,
        capacity,
        "%s\\%s",
        INDEX_ROOT,
        structures[structure].name
    );

    if (length < 0 || (size_t)length >= capacity) {
        fail("Ruta demasiado larga");
    }
}

static void clean_indexes(void) {
    DWORD attributes = GetFileAttributesA(INDEX_ROOT);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    char command[MAX_PATH_LENGTH];

    int length = snprintf(
        command,
        sizeof(command),
        "cmd /C rmdir /S /Q \"%s\"",
        INDEX_ROOT
    );

    if (length < 0 ||
        (size_t)length >= sizeof(command) ||
        system(command) != 0) {
        fail("No se pudieron limpiar los indices");
    }
}

static void free_postings(PostingList *postings) {
    if (postings) {
        free(postings->book_ids);
        free(postings);
    }
}

static InvertedIndex *create_books_index(
    const Book *books,
    int start,
    int end
) {
    InvertedIndex *index = create_index();

    if (!index) {
        fail("No se pudo crear el indice");
    }

    for (int i = start; i < end; i++) {
        char *text = read_book_text(&books[i]);

        if (!text) {
            free_index(index);
            fail("No se pudo leer un libro");
        }

        add_document(index, books[i].id, text);
        free(text);
    }

    return index;
}


static void save_structure(
    int structure,
    const InvertedIndex *index
) {
    char path[MAX_PATH_LENGTH];

    structure_path(structure, path, sizeof(path));

    CreateDirectoryA("index_benchmark", NULL);
    CreateDirectoryA(INDEX_ROOT, NULL);

    printf(
        "Guardando estructura: %s | Ruta: %s\n",
        structures[structure].name,
        path
    );
    fflush(stdout);

    if (!structures[structure].save(index, path)) {
        fprintf(
            stderr,
            "Fallo en estructura: %s | Ruta: %s\n",
            structures[structure].name,
            path
        );

        fail("Error guardando el indice");
    }
}

static void update_structure(
    int structure,
    const InvertedIndex *index
) {
    char path[MAX_PATH_LENGTH];

    structure_path(structure, path, sizeof(path));

    if (!structures[structure].update(index, path)) {
        fail("Error actualizando el indice");
    }
}

static PostingList *search_structure(
    int structure,
    const char *term
) {
    char path[MAX_PATH_LENGTH];

    structure_path(structure, path, sizeof(path));

    return structures[structure].search(path, term);
}

static void build_all_indexes(
    const Book *books,
    int count
) {
    clean_indexes();

    for (int i = 0; i < STRUCTURE_COUNT; i++) {
        InvertedIndex *index = create_books_index(
            books, 0, count
        );

        save_structure(i, index);
        free_index(index);
    }
}

static int contains_id(
    const PostingList *postings,
    int id
) {
    if (!postings) {
        return 0;
    }

    for (size_t i = 0; i < postings->count; i++) {
        if (postings->book_ids[i] == id) {
            return 1;
        }
    }

    return 0;
}

static void verify_structure(
    int structure,
    const InvertedIndex *expected
) {
    for (int q = 0; q < QUERY_COUNT; q++) {
        const char *term = queries[q];

        const PostingList *correct = search_term(
            expected, term
        );

        PostingList *actual = search_structure(
            structure, term
        );

        size_t expected_count = correct ? correct->count : 0;
        size_t actual_count = actual ? actual->count : 0;

        int valid = expected_count == actual_count;

        if (valid && actual) {
            for (size_t i = 0; i < actual->count; i++) {
                int id = actual->book_ids[i];

                if (!contains_id(correct, id)) {
                    valid = 0;
                    break;
                }

                for (size_t j = 0; j < i; j++) {
                    if (actual->book_ids[j] == id) {
                        valid = 0;
                        break;
                    }
                }

                if (!valid) {
                    break;
                }
            }
        }

        free_postings(actual);

        if (!valid) {
            fprintf(
                stderr,
                "Estructura: %s | Termino: %s\n",
                structures[structure].name,
                term
            );

            fail("Resultados de busqueda incorrectos");
        }
    }
}

static void verify_queries(
    const InvertedIndex *expected,
    const char *stage
) {
    printf(
        "\n========== QUERY CORRECTNESS: %s ==========\n",
        stage
    );

    for (int i = 0; i < STRUCTURE_COUNT; i++) {
        verify_structure(i, expected);
    }

    for (int q = 0; q < QUERY_COUNT; q++) {
        const PostingList *postings = search_term(
            expected, queries[q]
        );

        size_t count = postings ? postings->count : 0;

        printf(
            "%-15s | %zu documentos | OK\n",
            queries[q],
            count
        );
    }
}


static void benchmark_indexing(
    const Book *books,
    int count
) {
    printf("\n========== FULL INDEX BUILD ==========\n");

    FILE *csv = fopen("benchmark/c_index_build.csv", "w");

    if (!csv) {
        fail("No se pudo crear c_index_build.csv");
    }

    fprintf(
        csv,
        "structure,mean_s,std_s,books_per_second\n"
    );

    for (int i = 0; i < STRUCTURE_COUNT; i++) {
        double times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            clean_indexes();

            double start = now_seconds();

            InvertedIndex *index = create_books_index(
                books, 0, count
            );

            save_structure(i, index);

            times[r] = now_seconds() - start;

            free_index(index);
        }

        Stats stats = summarize(times, REPETITIONS);

        double books_per_second =
            stats.mean > 0 ? count / stats.mean : 0.0;

        printf(
            "%-12s | %.6f s | std: %.6f s | %.2f libros/s\n",
            structures[i].name,
            stats.mean,
            stats.std,
            books_per_second
        );

        fprintf(
            csv,
            "%s,%.6f,%.6f,%.2f\n",
            structures[i].name,
            stats.mean,
            stats.std,
            books_per_second
        );

        fflush(csv);
    }

    if (fclose(csv) != 0) {
        fail("Error cerrando c_index_build.csv");
    }
}



static void benchmark_queries(void) {
    printf("\n========== QUERY PERFORMANCE ==========\n");

    FILE *csv = fopen("benchmark/c_index_queries.csv", "w");

    if (!csv) {
        fail("No se pudo crear c_index_queries.csv");
    }

    fprintf(
        csv,
        "structure,mean_ms_per_query,std_ms_per_query\n"
    );

    for (int i = 0; i < STRUCTURE_COUNT; i++) {
        double times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            double start = now_seconds();

            for (int j = 0; j < QUERY_REPETITIONS; j++) {
                for (int q = 0; q < QUERY_COUNT; q++) {
                    PostingList *result = search_structure(
                        i, queries[q]
                    );

                    free_postings(result);
                }
            }

            double elapsed = now_seconds() - start;

            times[r] = elapsed * 1000.0 /
                (QUERY_REPETITIONS * QUERY_COUNT);
        }

        Stats stats = summarize(times, REPETITIONS);

        printf(
            "%-12s | %.6f ms/consulta | std: %.6f ms\n",
            structures[i].name,
            stats.mean,
            stats.std
        );

        fprintf(
            csv,
            "%s,%.6f,%.6f\n",
            structures[i].name,
            stats.mean,
            stats.std
        );

        fflush(csv);
    }

    if (fclose(csv) != 0) {
        fail("Error cerrando c_index_queries.csv");
    }
}


static void benchmark_incremental(
    const Book *books,
    int count,
    const InvertedIndex *expected
) {
    if (count < 10) {
        printf("\nNo hay suficientes libros para incremental.\n");
        return;
    }

    int base_count = count - 10;

    InvertedIndex *terms_index = create_books_index(
        books, base_count, count
    );

    size_t affected_terms = terms_index->count;

    free_index(terms_index);

    printf(
        "\n========== INCREMENTAL VS FULL REBUILD ==========\n"
    );

    printf("Libros iniciales: %d\n", base_count);
    printf("Libros nuevos: 10\n");

    printf("IDs nuevos: [");

    for (int i = base_count; i < count; i++) {
        printf(
            "%d%s",
            books[i].id,
            i < count - 1 ? ", " : ""
        );
    }

    printf("]\n");
    printf("Terminos afectados esperados: %zu\n", affected_terms);

    if (!CreateDirectoryA("benchmark", NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        fail("No se pudo crear benchmark");
    }

    FILE *csv = fopen(RESULTS_PATH, "wb");

    if (!csv) {
        fail("No se pudo crear el CSV");
    }

    fprintf(
        csv,
        "structure,incremental_mean_s,incremental_std_s,"
        "rebuild_mean_s,rebuild_std_s,affected_terms\n"
    );

    for (int i = 0; i < STRUCTURE_COUNT; i++) {
        double incremental_times[REPETITIONS];
        double rebuild_times[REPETITIONS];

        for (int r = 0; r < REPETITIONS; r++) {
            clean_indexes();

            InvertedIndex *base_index = create_books_index(
                books, 0, base_count
            );

            save_structure(i, base_index);
            free_index(base_index);

            double start = now_seconds();

            InvertedIndex *new_index = create_books_index(
                books, base_count, count
            );

            update_structure(i, new_index);

            incremental_times[r] = now_seconds() - start;

            free_index(new_index);

            verify_structure(i, expected);

            clean_indexes();

            start = now_seconds();

            InvertedIndex *full_index = create_books_index(
                books, 0, count
            );

            save_structure(i, full_index);

            rebuild_times[r] = now_seconds() - start;

            free_index(full_index);
        }

        Stats incremental = summarize(
            incremental_times, REPETITIONS
        );

        Stats rebuild = summarize(
            rebuild_times, REPETITIONS
        );

        printf("\n%s\n", structures[i].name);

        printf(
            "Actualizacion incremental: %.6f s (std: %.6f)\n",
            incremental.mean,
            incremental.std
        );

        printf(
            "Reconstruccion completa: %.6f s (std: %.6f)\n",
            rebuild.mean,
            rebuild.std
        );

        printf(
            "Terminos afectados: %zu\n",
            affected_terms
        );

        fprintf(
            csv,
            "%s,%.6f,%.6f,%.6f,%.6f,%zu\n",
            structures[i].name,
            incremental.mean,
            incremental.std,
            rebuild.mean,
            rebuild.std,
            affected_terms
        );
    }

    if (fclose(csv) != 0) {
        fail("Error cerrando el CSV");
    }

    printf(
        "\nVerificacion incremental: OK en las tres estructuras\n"
    );

    printf("\nResultados guardados en: %s\n", RESULTS_PATH);

}

static void scan_storage(
    const char *directory,
    Storage *storage
) {
    char pattern[MAX_PATH_LENGTH];

    int length = snprintf(
        pattern,
        sizeof(pattern),
        "%s\\*",
        directory
    );

    if (length < 0 || (size_t)length >= sizeof(pattern)) {
        fail("Ruta demasiado larga");
    }

    WIN32_FIND_DATAA data;

    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return;
        }

        fail("Error examinando almacenamiento");
    }

    do {
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0) {
            continue;
        }

        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            storage->directories++;

            char child[MAX_PATH_LENGTH];

            length = snprintf(
                child,
                sizeof(child),
                "%s\\%s",
                directory,
                data.cFileName
            );

            if (length < 0 ||
                (size_t)length >= sizeof(child)) {
                FindClose(handle);
                fail("Ruta demasiado larga");
            }

            scan_storage(child, storage);
        } else {
            storage->files++;

            unsigned long long size =
                ((unsigned long long)data.nFileSizeHigh << 32) |
                data.nFileSizeLow;

            storage->bytes += size;
        }
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
}


static void benchmark_storage(void) {
    printf("\n========== STORAGE ==========\n");

    FILE *csv = fopen("benchmark/c_index_storage.csv", "w");

    if (!csv) {
        fail("No se pudo crear c_index_storage.csv");
    }

    fprintf(
        csv,
        "structure,size_mb,files,directories\n"
    );

    for (int i = 0; i < STRUCTURE_COUNT; i++) {
        char path[MAX_PATH_LENGTH];

        structure_path(i, path, sizeof(path));

        Storage storage = {0, 0, 0};

        scan_storage(path, &storage);

        double size_mb = storage.bytes / (1024.0 * 1024.0);

        printf(
            "%-12s | %.2f MB | %llu archivos | %llu directorios\n",
            structures[i].name,
            size_mb,
            storage.files,
            storage.directories
        );

        fprintf(
            csv,
            "%s,%.6f,%llu,%llu\n",
            structures[i].name,
            size_mb,
            storage.files,
            storage.directories
        );

        fflush(csv);
    }

    if (fclose(csv) != 0) {
        fail("Error cerrando c_index_storage.csv");
    }
}


int main(void) {
    if (!CreateDirectoryA("benchmark", NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        fail("No se pudo crear la carpeta benchmark");
    }

    int count = 0;

    Book *books = load_books(SAMPLE_PATH, &count);

    printf("========================================\n");
    printf("     INVERTED INDEX BENCHMARK - C\n");
    printf("========================================\n");
    printf("Libros cargados: %d\n", count);
    printf("Repeticiones: %d\n", REPETITIONS);

    if (!books || count == 0) {
        printf("No se encontraron libros en sample_data.\n");
        free_books(books, count);
        return EXIT_FAILURE;
    }

    InvertedIndex *expected = create_books_index(
        books, 0, count
    );

    benchmark_indexing(books, count);

    build_all_indexes(books, count);

    benchmark_queries();

    verify_queries(expected, "FULL BUILD");

    benchmark_incremental(books, count, expected);

    build_all_indexes(books, count);

    verify_queries(expected, "AFTER REBUILD");

    benchmark_storage();

    free_index(expected);
    free_books(books, count);

    printf("\n========================================\n");
    printf("          BENCHMARK COMPLETADO\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
