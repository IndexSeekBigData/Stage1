
import shutil
import statistics
import time
from pathlib import Path

from time_based import save_book_time_based
from book_based import save_book_book_based
from range_based import save_book_range_based

PROJECT_ROOT = Path(__file__).resolve().parents[3]

SAMPLE_DATA_PATH = PROJECT_ROOT / "sample_data"
BENCHMARK_PATH = PROJECT_ROOT / "datalake_benchmark"

REPETITIONS = 5
LOOKUP_REPETITIONS = 100

DATE = "20261004"
HOUR = "22"

STRUCTURES = (
    "time_based",
    "book_based",
    "range_based"
)


def load_books():
    books = []

    for body_path in sorted(SAMPLE_DATA_PATH.glob("*.body.txt")):
        book_id = body_path.name.split(".")[0]
        header_path = SAMPLE_DATA_PATH / f"{book_id}.header.txt"

        if not header_path.is_file():
            continue

        header = header_path.read_text(
            encoding="utf-8",
            errors="ignore"
        )

        body = body_path.read_text(
            encoding="utf-8",
            errors="ignore"
        )

        books.append((book_id, header, body))

    return books


def clean_structure(structure):
    path = BENCHMARK_PATH / structure

    if path.exists():
        shutil.rmtree(path)


def clean_all():
    for structure in STRUCTURES:
        clean_structure(structure)


def save_book(structure, book):
    book_id, header, body = book

    if structure == "time_based":
        save_book_time_based(
            book_id, header, body, DATE, HOUR
        )
    elif structure == "book_based":
        save_book_book_based(
            book_id, header, body
        )
    elif structure == "range_based":
        save_book_range_based(
            book_id, header, body
        )
    else:
        raise ValueError(
            f"Estructura desconocida: {structure}"
        )


def save_books(structure, books):
    for book in books:
        save_book(structure, book)


def save_all_books(books):
    for structure in STRUCTURES:
        save_books(structure, books)


def print_timing(name, times, unit="s"):
    average = statistics.mean(times)
    deviation = (
        statistics.stdev(times)
        if len(times) > 1 else 0.0
    )

    print(
        f"{name:12} | "
        f"{average:.6f} {unit} | "
        f"desviación: {deviation:.6f} {unit}"
    )

    return average


def benchmark_write(books):
    print("\n========== WRITE / THROUGHPUT ==========")

    for structure in STRUCTURES:
        times = []

        for _ in range(REPETITIONS):
            clean_structure(structure)

            start = time.perf_counter()
            save_books(structure, books)
            elapsed = time.perf_counter() - start

            times.append(elapsed)

        average = print_timing(structure, times)

        print(
            f"{'':12}   "
            f"{len(books) / average:.2f} libros/s"
        )


def lookup_time_based(book_id):
    base = BENCHMARK_PATH / "time_based"

    return next(
        base.rglob(f"{book_id}.body.txt"),
        None
    )


def lookup_book_based(book_id):
    path = (
            BENCHMARK_PATH
            / "book_based"
            / str(book_id)
            / f"{book_id}.body.txt"
    )

    return path if path.is_file() else None


def lookup_range_based(book_id):
    book_number = int(book_id)
    range_start = (book_number // 1000) * 1000
    range_end = range_start + 999

    path = (
            BENCHMARK_PATH
            / "range_based"
            / f"{range_start}-{range_end}"
            / f"{book_id}.body.txt"
    )

    return path if path.is_file() else None


LOOKUP_FUNCTIONS = {
    "time_based": lookup_time_based,
    "book_based": lookup_book_based,
    "range_based": lookup_range_based
}


def benchmark_lookup(books):
    print("\n========== LOOKUP ==========")

    book_ids = [book[0] for book in books]

    clean_all()
    save_all_books(books)

    for structure, function in LOOKUP_FUNCTIONS.items():
        times = []

        for _ in range(REPETITIONS):
            start = time.perf_counter()

            for _ in range(LOOKUP_REPETITIONS):
                for book_id in book_ids:
                    result = function(book_id)

                    if result is None:
                        raise RuntimeError(
                            f"Libro {book_id} no encontrado"
                        )

            elapsed = time.perf_counter() - start

            total_searches = (
                    LOOKUP_REPETITIONS * len(book_ids)
            )

            times.append(
                elapsed * 1000 / total_searches
            )

        print_timing(
            structure,
            times,
            "ms/búsqueda"
        )


def benchmark_incremental(books):
    if len(books) < 10:
        print("\nNo hay suficientes libros.")
        return

    base_books = books[:-10]
    new_books = books[-10:]

    print("\n========== INCREMENTAL INSERTION ==========")
    print(f"Libros iniciales: {len(base_books)}")
    print(f"Libros añadidos: {len(new_books)}")
    print(
        "IDs añadidos:",
        [book[0] for book in new_books]
    )

    for structure in STRUCTURES:
        times = []

        for _ in range(REPETITIONS):
            clean_structure(structure)
            save_books(structure, base_books)

            start = time.perf_counter()
            save_books(structure, new_books)
            elapsed = time.perf_counter() - start

            times.append(elapsed)

        print_timing(structure, times)


def discover_downloaded_ids(structure):
    base = BENCHMARK_PATH / structure

    return {
        path.name.split(".")[0]
        for path in base.rglob("*.body.txt")
        if path.is_file()
    }


def discover_pending_books(structure, indexed_ids):
    downloaded_ids = discover_downloaded_ids(structure)

    return sorted(
        downloaded_ids - indexed_ids,
        key=int
    )


def benchmark_discovery(books):
    if len(books) < 10:
        print("\nNo hay suficientes libros.")
        return

    indexed_ids = {
        book[0]
        for book in books[:-10]
    }

    expected_ids = sorted(
        [book[0] for book in books[-10:]],
        key=int
    )

    print("\n========== PENDING BOOK DISCOVERY ==========")
    print(f"Libros descargados: {len(books)}")
    print(f"Libros indexados: {len(indexed_ids)}")
    print(f"IDs pendientes esperados: {expected_ids}")

    for structure in STRUCTURES:
        clean_structure(structure)
        save_books(structure, books)

        times = []
        discovered_ids = None

        for _ in range(REPETITIONS):
            start = time.perf_counter()

            result = discover_pending_books(
                structure,
                indexed_ids
            )

            elapsed = time.perf_counter() - start
            times.append(elapsed)

            if result != expected_ids:
                raise AssertionError(
                    f"{structure}: IDs incorrectos. "
                    f"Esperados: {expected_ids}. "
                    f"Obtenidos: {result}"
                )

            discovered_ids = result

        print_timing(structure, times)
        print(f"{'':12}   IDs: {discovered_ids}")


def directory_size(path):
    return sum(
        file.stat().st_size
        for file in path.rglob("*")
        if file.is_file()
    )


def count_files(path):
    return sum(
        1
        for file in path.rglob("*")
        if file.is_file()
    )


def count_directories(path):
    return sum(
        1
        for directory in path.rglob("*")
        if directory.is_dir()
    )


def benchmark_storage(books):
    clean_all()
    save_all_books(books)

    print("\n========== STORAGE ==========")

    for structure in STRUCTURES:
        path = BENCHMARK_PATH / structure

        size_mb = (
                directory_size(path) / (1024 * 1024)
        )

        files = count_files(path)
        directories = count_directories(path)

        print(
            f"{structure:12} | "
            f"{size_mb:.2f} MB | "
            f"{files} archivos | "
            f"{directories} directorios"
        )


def benchmark_recovery(books):
    if not books:
        return

    interrupted_book = books[0]
    interrupted_id = interrupted_book[0]

    expected_ids = {book[0] for book in books}

    print("\n========== RECOVERY AFTER INTERRUPTION ==========")
    print(f"Libros esperados: {len(expected_ids)}")
    print(f"Libro interrumpido: {interrupted_id}")
    print("Interrupción: después de escribir header y antes de body")

    for structure in STRUCTURES:
        clean_structure(structure)
        save_books(structure, books)

        body_path = LOOKUP_FUNCTIONS[structure](
            interrupted_id
        )

        if body_path is None:
            raise RuntimeError(
                f"No se encontró el body del libro {interrupted_id}"
            )

        header_path = (
                body_path.parent
                / f"{interrupted_id}.header.txt"
        )

        body_path.unlink()

        if not header_path.is_file() or body_path.exists():
            raise AssertionError(
                f"{structure}: estado de interrupción incorrecto"
            )

        start = time.perf_counter()
        save_book(structure, interrupted_book)
        elapsed = time.perf_counter() - start

        header_files = list(
            (BENCHMARK_PATH / structure).rglob(
                "*.header.txt"
            )
        )

        body_files = list(
            (BENCHMARK_PATH / structure).rglob(
                "*.body.txt"
            )
        )

        header_ids = [
            path.name.split(".")[0]
            for path in header_files
        ]

        body_ids = [
            path.name.split(".")[0]
            for path in body_files
        ]

        missing_header_ids = sorted(
            expected_ids - set(header_ids),
            key=int
        )

        missing_body_ids = sorted(
            expected_ids - set(body_ids),
            key=int
        )

        duplicate_header_ids = sorted(
            {
                book_id
                for book_id in header_ids
                if header_ids.count(book_id) > 1
            },
            key=int
        )

        duplicate_body_ids = sorted(
            {
                book_id
                for book_id in body_ids
                if body_ids.count(book_id) > 1
            },
            key=int
        )

        complete_pairs = len(
            set(header_ids) & set(body_ids)
        )

        valid = (
                not missing_header_ids
                and not missing_body_ids
                and not duplicate_header_ids
                and not duplicate_body_ids
                and len(header_files) == len(expected_ids)
                and len(body_files) == len(expected_ids)
                and complete_pairs == len(expected_ids)
        )

        print(f"\n{structure}")
        print(f"Tiempo de recuperación: {elapsed:.6f} s")
        print(f"Headers: {len(header_files)}")
        print(f"Bodies: {len(body_files)}")
        print(f"Pares completos: {complete_pairs}")
        print(f"IDs sin header: {missing_header_ids}")
        print(f"IDs sin body: {missing_body_ids}")
        print(f"IDs header duplicados: {duplicate_header_ids}")
        print(f"IDs body duplicados: {duplicate_body_ids}")
        print(f"Recuperación correcta: {valid}")

        if not valid:
            raise AssertionError(
                f"Falló la recuperación de {structure}"
            )


def benchmark_scalability(books):
    print("\n========== SCALABILITY ==========")

    corpus_sizes = (10, 25, 50, 100)
    available_sizes = [
        size
        for size in corpus_sizes
        if size <= len(books)
    ]

    results_path = PROJECT_ROOT / "benchmark"
    results_path.mkdir(parents=True, exist_ok=True)

    output_path = results_path / "datalake_scalability.csv"

    rows = [
        "structure,books,write_mean_s,write_std_s,"
        "throughput_books_s,lookup_mean_ms,lookup_std_ms,"
        "storage_mb,files,directories"
    ]

    for size in available_sizes:
        selected_books = books[:size]
        book_ids = [book[0] for book in selected_books]

        print(f"\n========== CORPUS: {size} BOOKS ==========")

        for structure in STRUCTURES:
            write_times = []
            lookup_times = []

            clean_structure(structure)
            save_books(structure, selected_books)

            clean_structure(structure)

            for _ in range(REPETITIONS):
                clean_structure(structure)

                start = time.perf_counter()
                save_books(structure, selected_books)
                elapsed = time.perf_counter() - start

                write_times.append(elapsed)

            lookup_function = LOOKUP_FUNCTIONS[structure]

            for book_id in book_ids:
                result = lookup_function(book_id)

                if result is None:
                    raise AssertionError(
                        f"{structure}: libro {book_id} no encontrado"
                    )

            for _ in range(REPETITIONS):
                start = time.perf_counter()

                for _ in range(LOOKUP_REPETITIONS):
                    for book_id in book_ids:
                        result = lookup_function(book_id)

                        if result is None:
                            raise AssertionError(
                                f"{structure}: libro {book_id} no encontrado"
                            )

                elapsed = time.perf_counter() - start

                total_searches = (
                        LOOKUP_REPETITIONS * len(book_ids)
                )

                lookup_times.append(
                    elapsed * 1000 / total_searches
                )

            path = BENCHMARK_PATH / structure

            write_mean = statistics.mean(write_times)
            write_std = statistics.stdev(write_times)

            lookup_mean = statistics.mean(lookup_times)
            lookup_std = statistics.stdev(lookup_times)

            throughput = size / write_mean
            storage_mb = directory_size(path) / (1024 * 1024)
            files = count_files(path)
            directories = count_directories(path)

            print(f"\n{structure}")
            print(f"Escritura: {write_mean:.6f} s")
            print(f"Desviación escritura: {write_std:.6f} s")
            print(f"Rendimiento: {throughput:.2f} libros/s")
            print(f"Búsqueda: {lookup_mean:.6f} ms")
            print(f"Desviación búsqueda: {lookup_std:.6f} ms")
            print(f"Almacenamiento: {storage_mb:.2f} MB")
            print(f"Archivos: {files}")
            print(f"Directorios: {directories}")

            rows.append(
                f"{structure},{size},"
                f"{write_mean:.6f},{write_std:.6f},"
                f"{throughput:.2f},"
                f"{lookup_mean:.6f},{lookup_std:.6f},"
                f"{storage_mb:.4f},{files},{directories}"
            )

    output_path.write_text(
        "\n".join(rows) + "\n",
        encoding="utf-8"
    )

    print(f"\nResultados guardados en: {output_path}")


def main():
    books = load_books()

    print("========================================")
    print("      DATALAKE BENCHMARK - PYTHON")
    print("========================================")

    print(f"Libros cargados: {len(books)}")
    print(f"Repeticiones: {REPETITIONS}")
    print(f"Sample data: {SAMPLE_DATA_PATH}")
    print(f"Benchmark path: {BENCHMARK_PATH}")

    if not books:
        print("No se encontraron libros en sample_data.")
        return

    benchmark_write(books)
    benchmark_lookup(books)
    benchmark_incremental(books)
    benchmark_discovery(books)
    benchmark_storage(books)
    benchmark_recovery(books)
    benchmark_scalability(books)

    print("\n========================================")
    print("          BENCHMARK COMPLETADO")
    print("========================================")


if __name__ == "__main__":
    main()
