import shutil
import time
from pathlib import Path

from common import tokenize
import monolithic_json
import term_files
import hierarchical


PROJECT_ROOT = Path(__file__).resolve().parents[3]
SAMPLE_DATA_PATH = PROJECT_ROOT / "sample_data"
INDEX_BENCHMARK_PATH = PROJECT_ROOT / "index_benchmark"

REPETITIONS = 3


def load_books():
    books = []

    for body_path in SAMPLE_DATA_PATH.glob("*.body.txt"):
        book_id = body_path.name.split(".")[0]

        body = body_path.read_text(
            encoding="utf-8",
            errors="ignore"
        )

        books.append((book_id, body))

    return books


def clean_indexes():
    if INDEX_BENCHMARK_PATH.exists():
        shutil.rmtree(INDEX_BENCHMARK_PATH)


def benchmark_indexing(books):
    structures = {
        "monolithic": monolithic_json,
        "term_files": term_files,
        "hierarchical": hierarchical
    }

    results = {}

    print("\n========== INDEXING =========")

    for name, module in structures.items():
        times = []

        for _ in range(REPETITIONS):
            clean_indexes()

            start = time.perf_counter()

            module.build_index(
                books,
                tokenize
            )

            elapsed = time.perf_counter() - start
            times.append(elapsed)

        average = sum(times) / len(times)
        results[name] = average

        throughput = len(books) / average

        print(
            f"{name:12} | "
            f"{average:.6f} s | "
            f"{throughput:.2f} libros/s"
        )

    return results


def build_all_indexes(books):
    clean_indexes()

    monolithic_json.build_index(
        books,
        tokenize
    )

    term_files.build_index(
        books,
        tokenize
    )

    hierarchical.build_index(
        books,
        tokenize
    )


def benchmark_queries():
    structures = {
        "monolithic": monolithic_json,
        "term_files": term_files,
        "hierarchical": hierarchical
    }

    queries = [
        "the",
        "and",
        "love",
        "man",
        "woman",
        "time",
        "house",
        "book",
        "zzzznotfound"
    ]

    repetitions = 100

    print("\n========== QUERY PERFORMANCE ==========")

    for name, module in structures.items():
        start = time.perf_counter()

        total_queries = 0

        for _ in range(repetitions):
            for term in queries:
                module.search_term(term)
                total_queries += 1

        elapsed = time.perf_counter() - start
        average = elapsed / total_queries

        print(
            f"{name:12} | "
            f"{average * 1000:.6f} ms/consulta"
        )


def benchmark_incremental(books):
    if len(books) < 10:
        print(
            "\nNo hay suficientes libros "
            "para incremental."
        )
        return

    base_books = books[:-10]

    structures = {
        "monolithic": monolithic_json,
        "term_files": term_files,
        "hierarchical": hierarchical
    }

    print("\n========== INCREMENTAL UPDATE ==========")

    for name, module in structures.items():
        clean_indexes()

        module.build_index(
            base_books,
            tokenize
        )

        start = time.perf_counter()

        module.build_index(
            books,
            tokenize
        )

        elapsed = time.perf_counter() - start

        print(
            f"{name:12} | "
            f"{elapsed:.6f} s"
        )


def directory_size(path):
    total = 0

    if not path.exists():
        return 0

    for file in path.rglob("*"):
        if file.is_file():
            total += file.stat().st_size

    return total


def count_files(path):
    if not path.exists():
        return 0

    return sum(
        1
        for file in path.rglob("*")
        if file.is_file()
    )


def count_directories(path):
    if not path.exists():
        return 0

    return sum(
        1
        for directory in path.rglob("*")
        if directory.is_dir()
    )


def benchmark_storage():
    print("\n========== STORAGE ==========")

    paths = {
        "monolithic":
            INDEX_BENCHMARK_PATH / "monolithic",

        "term_files":
            INDEX_BENCHMARK_PATH / "term_files",

        "hierarchical":
            INDEX_BENCHMARK_PATH / "hierarchical"
    }

    for name, path in paths.items():
        size_mb = (
                directory_size(path)
                / (1024 * 1024)
        )

        files = count_files(path)
        directories = count_directories(path)

        print(
            f"{name:12} | "
            f"{size_mb:.2f} MB | "
            f"{files} archivos | "
            f"{directories} directorios"
        )


if __name__ == "__main__":
    books = load_books()

    print("========================================")
    print("    INVERTED INDEX BENCHMARK - PYTHON")
    print("========================================")

    print(f"Libros cargados: {len(books)}")

    if not books:
        print("No se encontraron libros en sample_data.")
        exit()

    benchmark_indexing(books)

    build_all_indexes(books)

    benchmark_queries()

    benchmark_incremental(books)

    build_all_indexes(books)

    benchmark_storage()

    print("\n========================================")
    print("          BENCHMARK COMPLETADO")
    print("========================================")