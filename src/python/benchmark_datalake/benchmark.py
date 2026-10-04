import shutil
import time
from pathlib import Path

from time_based import save_book_time_based
from book_based import save_book_book_based
from range_based import save_book_range_based


# --------------------------------------------------
# RUTAS
# --------------------------------------------------

PROJECT_ROOT = Path(__file__).resolve().parents[3]

SAMPLE_DATA_PATH = PROJECT_ROOT / "sample_data"
BENCHMARK_PATH = PROJECT_ROOT / "datalake_benchmark"

REPETITIONS = 5


# --------------------------------------------------
# CARGAR DATASET
# --------------------------------------------------

def load_books():
    books = []

    for body_path in SAMPLE_DATA_PATH.glob("*.body.txt"):
        book_id = body_path.name.split(".")[0]

        header_path = SAMPLE_DATA_PATH / f"{book_id}.header.txt"

        if header_path.exists():
            header = header_path.read_text(
                encoding="utf-8",
                errors="ignore"
            )

            body = body_path.read_text(
                encoding="utf-8",
                errors="ignore"
            )

            books.append(
                (book_id, header, body)
            )

    return books


# --------------------------------------------------
# LIMPIAR ESTRUCTURAS
# --------------------------------------------------

def clean_structure(structure):
    path = BENCHMARK_PATH / structure

    if path.exists():
        shutil.rmtree(path)


def clean_all():
    clean_structure("time_based")
    clean_structure("book_based")
    clean_structure("range_based")


# --------------------------------------------------
# GUARDAR TODOS LOS LIBROS
# --------------------------------------------------

def save_all_books(books):
    date = "20261004"
    hour = "22"

    for book_id, header, body in books:
        save_book_time_based(
            book_id,
            header,
            body,
            date,
            hour
        )

    for book_id, header, body in books:
        save_book_book_based(
            book_id,
            header,
            body
        )

    for book_id, header, body in books:
        save_book_range_based(
            book_id,
            header,
            body
        )


# --------------------------------------------------
# 1. WRITE / THROUGHPUT
# --------------------------------------------------

def benchmark_write(books):
    date = "20261004"
    hour = "22"

    results = {
        "time_based": [],
        "book_based": [],
        "range_based": []
    }

    for _ in range(REPETITIONS):

        # TIME-BASED
        clean_structure("time_based")

        start = time.perf_counter()

        for book_id, header, body in books:
            save_book_time_based(
                book_id,
                header,
                body,
                date,
                hour
            )

        elapsed = time.perf_counter() - start
        results["time_based"].append(elapsed)

        # BOOK-BASED
        clean_structure("book_based")

        start = time.perf_counter()

        for book_id, header, body in books:
            save_book_book_based(
                book_id,
                header,
                body
            )

        elapsed = time.perf_counter() - start
        results["book_based"].append(elapsed)

        # RANGE-BASED
        clean_structure("range_based")

        start = time.perf_counter()

        for book_id, header, body in books:
            save_book_range_based(
                book_id,
                header,
                body
            )

        elapsed = time.perf_counter() - start
        results["range_based"].append(elapsed)

    print("\n========== WRITE / THROUGHPUT ==========")

    for structure, times in results.items():
        average = sum(times) / len(times)

        throughput = len(books) / average

        print(
            f"{structure:12} | "
            f"{average:.6f} s | "
            f"{throughput:.2f} libros/s"
        )


# --------------------------------------------------
# 2. LOOKUP
# --------------------------------------------------

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

    if path.exists():
        return path

    return None


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

    if path.exists():
        return path

    return None


def benchmark_lookup(books):
    book_ids = [
        book[0]
        for book in books
    ]

    lookup_repetitions = 100

    functions = {
        "time_based": lookup_time_based,
        "book_based": lookup_book_based,
        "range_based": lookup_range_based
    }

    print("\n========== LOOKUP ==========")

    for name, function in functions.items():
        start = time.perf_counter()

        searches = 0

        for _ in range(lookup_repetitions):

            for book_id in book_ids:
                function(book_id)
                searches += 1

        elapsed = time.perf_counter() - start

        average = elapsed / searches

        print(
            f"{name:12} | "
            f"{average * 1000:.6f} ms/búsqueda"
        )


# --------------------------------------------------
# 3. INCREMENTAL PROCESSING
# --------------------------------------------------

def benchmark_incremental(books):
    if len(books) < 10:
        print(
            "\nNo hay suficientes libros "
            "para el benchmark incremental."
        )
        return

    base_books = books[:-10]
    new_books = books[-10:]

    date = "20261004"
    hour = "22"

    print("\n========== INCREMENTAL ==========")

    # TIME-BASED
    clean_structure("time_based")

    for book_id, header, body in base_books:
        save_book_time_based(
            book_id,
            header,
            body,
            date,
            hour
        )

    start = time.perf_counter()

    for book_id, header, body in new_books:
        save_book_time_based(
            book_id,
            header,
            body,
            date,
            hour
        )

    elapsed = time.perf_counter() - start

    print(
        f"time_based   | "
        f"{elapsed:.6f} s para 10 libros"
    )

    # BOOK-BASED
    clean_structure("book_based")

    for book_id, header, body in base_books:
        save_book_book_based(
            book_id,
            header,
            body
        )

    start = time.perf_counter()

    for book_id, header, body in new_books:
        save_book_book_based(
            book_id,
            header,
            body
        )

    elapsed = time.perf_counter() - start

    print(
        f"book_based   | "
        f"{elapsed:.6f} s para 10 libros"
    )

    # RANGE-BASED
    clean_structure("range_based")

    for book_id, header, body in base_books:
        save_book_range_based(
            book_id,
            header,
            body
        )

    start = time.perf_counter()

    for book_id, header, body in new_books:
        save_book_range_based(
            book_id,
            header,
            body
        )

    elapsed = time.perf_counter() - start

    print(
        f"range_based  | "
        f"{elapsed:.6f} s para 10 libros"
    )


# --------------------------------------------------
# 4. STORAGE OVERHEAD
# --------------------------------------------------

def directory_size(path):
    total = 0

    for file in path.rglob("*"):

        if file.is_file():
            total += file.stat().st_size

    return total


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

    structures = [
        "time_based",
        "book_based",
        "range_based"
    ]

    for structure in structures:
        path = BENCHMARK_PATH / structure

        size_mb = (
                directory_size(path)
                / (1024 * 1024)
        )

        files = count_files(path)
        directories = count_directories(path)

        print(
            f"{structure:12} | "
            f"{size_mb:.2f} MB | "
            f"{files} archivos | "
            f"{directories} directorios"
        )


# --------------------------------------------------
# 5. RECOVERY BEHAVIOR
# --------------------------------------------------

def benchmark_recovery(books):
    if not books:
        return

    clean_all()
    save_all_books(books)

    book_id, header, body = books[0]

    date = "20261004"
    hour = "22"

    print("\n========== RECOVERY ==========")

    # TIME-BASED
    path = lookup_time_based(book_id)

    if path:
        path.unlink()

    start = time.perf_counter()

    save_book_time_based(
        book_id,
        header,
        body,
        date,
        hour
    )

    elapsed = time.perf_counter() - start

    print(
        f"time_based   | "
        f"{elapsed:.6f} s"
    )

    # BOOK-BASED
    path = lookup_book_based(book_id)

    if path:
        path.unlink()

    start = time.perf_counter()

    save_book_book_based(
        book_id,
        header,
        body
    )

    elapsed = time.perf_counter() - start

    print(
        f"book_based   | "
        f"{elapsed:.6f} s"
    )

    # RANGE-BASED
    path = lookup_range_based(book_id)

    if path:
        path.unlink()

    start = time.perf_counter()

    save_book_range_based(
        book_id,
        header,
        body
    )

    elapsed = time.perf_counter() - start

    print(
        f"range_based  | "
        f"{elapsed:.6f} s"
    )


# --------------------------------------------------
# MAIN
# --------------------------------------------------

if __name__ == "__main__":

    books = load_books()

    print("========================================")
    print("      DATALAKE BENCHMARK - PYTHON")
    print("========================================")

    print(f"Libros cargados: {len(books)}")

    print(
        f"Sample data: "
        f"{SAMPLE_DATA_PATH}"
    )

    print(
        f"Benchmark path: "
        f"{BENCHMARK_PATH}"
    )

    if not books:
        print(
            "No se encontraron libros "
            "en sample_data."
        )

        exit()

    benchmark_write(books)

    # Crear las estructuras completas
    # antes de medir las búsquedas
    clean_all()
    save_all_books(books)

    benchmark_lookup(books)

    benchmark_incremental(books)

    benchmark_storage(books)

    benchmark_recovery(books)

    print("\n========================================")
    print("          BENCHMARK COMPLETADO")
    print("========================================")