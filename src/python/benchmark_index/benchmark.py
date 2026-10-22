
import json
import shutil
import statistics
import time
from pathlib import Path

from common import tokenize
import monolithic_json
import term_files
import hierarchical


PROJECT_ROOT = Path(__file__).resolve().parents[3]
SAMPLE_DATA_PATH = PROJECT_ROOT / "sample_data"
INDEX_BENCHMARK_PATH = PROJECT_ROOT / "index_benchmark"
RESULTS_PATH = PROJECT_ROOT / "benchmark"

REPETITIONS = 3
QUERY_REPETITIONS = 100

STRUCTURES = {
    "monolithic": monolithic_json,
    "term_files": term_files,
    "hierarchical": hierarchical
}

QUERIES = [
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


def load_books():
    books = []

    for body_path in sorted(SAMPLE_DATA_PATH.glob("*.body.txt")):
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


def build_all_indexes(books):
    clean_indexes()

    for module in STRUCTURES.values():
        module.build_index(books, tokenize)


def timing_summary(times):
    return (
        statistics.mean(times),
        statistics.stdev(times) if len(times) > 1 else 0.0
    )


def benchmark_indexing(books):
    print("\n========== FULL INDEX BUILD ==========")

    for name, module in STRUCTURES.items():
        times = []

        for _ in range(REPETITIONS):
            clean_indexes()

            start = time.perf_counter()
            module.build_index(books, tokenize)
            elapsed = time.perf_counter() - start

            times.append(elapsed)

        average, deviation = timing_summary(times)

        print(
            f"{name:12} | "
            f"{average:.6f} s | "
            f"std: {deviation:.6f} s | "
            f"{len(books) / average:.2f} libros/s"
        )


def benchmark_queries():
    print("\n========== QUERY PERFORMANCE ==========")
    print(f"Consultas: {QUERIES}")

    for name, module in STRUCTURES.items():
        times = []

        for _ in range(REPETITIONS):
            start = time.perf_counter()

            for _ in range(QUERY_REPETITIONS):
                for term in QUERIES:
                    module.search_term(term)

            elapsed = time.perf_counter() - start
            total = QUERY_REPETITIONS * len(QUERIES)

            times.append(elapsed * 1000 / total)

        average, deviation = timing_summary(times)

        print(
            f"{name:12} | "
            f"{average:.6f} ms/consulta | "
            f"std: {deviation:.6f} ms"
        )


def expected_postings(books):
    index = {}

    for book_id, body in books:
        for term in set(tokenize(body)):
            index.setdefault(term, set()).add(str(book_id))

    return index


def verify_queries(books, stage):
    expected = expected_postings(books)

    print(f"\n========== QUERY CORRECTNESS: {stage} ==========")

    for term in QUERIES:
        expected_ids = expected.get(term, set())
        results = {}

        for name, module in STRUCTURES.items():
            returned_ids = module.search_term(term)
            returned_set = set(returned_ids)

            if len(returned_ids) != len(returned_set):
                raise AssertionError(
                    f"{name}: IDs duplicados para '{term}'"
                )

            if returned_set != expected_ids:
                raise AssertionError(
                    f"{name}: resultados incorrectos para '{term}'. "
                    f"Esperados: {sorted(expected_ids, key=int)}. "
                    f"Obtenidos: {sorted(returned_set, key=int)}"
                )

            results[name] = returned_set

        if len({frozenset(ids) for ids in results.values()}) != 1:
            raise AssertionError(
                f"Las estructuras no coinciden para '{term}'"
            )

        print(
            f"{term:15} | "
            f"{len(expected_ids)} documentos | "
            f"OK"
        )



def benchmark_incremental(books):
    if len(books) < 10:
        print("\nNo hay suficientes libros para incremental.")
        return

    base_books = books[:-10]
    new_books = books[-10:]

    expected_terms = set()

    for _, body in new_books:
        expected_terms.update(tokenize(body))

    expected = expected_postings(books)

    print("\n========== INCREMENTAL VS FULL REBUILD ==========")
    print(f"Libros iniciales: {len(base_books)}")
    print(f"Libros nuevos: {len(new_books)}")
    print(f"IDs nuevos: {[book_id for book_id, _ in new_books]}")
    print(f"Términos afectados esperados: {len(expected_terms)}")

    results = []

    for name, module in STRUCTURES.items():
        incremental_times = []
        rebuild_times = []

        for _ in range(REPETITIONS):
            clean_indexes()
            module.build_index(base_books, tokenize)

            start = time.perf_counter()

            affected_terms = module.update_index(
                new_books,
                tokenize
            )

            incremental_elapsed = time.perf_counter() - start

            if set(affected_terms) != expected_terms:
                raise AssertionError(
                    f"{name}: términos afectados incorrectos"
                )

            incremental_times.append(incremental_elapsed)

            for term in QUERIES:
                returned_ids = module.search_term(term)
                expected_ids = expected.get(term, set())

                if (
                        len(returned_ids) != len(set(returned_ids))
                        or set(returned_ids) != expected_ids
                ):
                    raise AssertionError(
                        f"{name}: actualización incremental incorrecta "
                        f"para el término '{term}'"
                    )

            start = time.perf_counter()

            module.build_index(books, tokenize)

            rebuild_elapsed = time.perf_counter() - start

            rebuild_times.append(rebuild_elapsed)

        incremental_mean, incremental_std = timing_summary(
            incremental_times
        )

        rebuild_mean, rebuild_std = timing_summary(
            rebuild_times
        )

        results.append({
            "structure": name,
            "incremental_mean_s": incremental_mean,
            "incremental_std_s": incremental_std,
            "rebuild_mean_s": rebuild_mean,
            "rebuild_std_s": rebuild_std,
            "affected_terms": len(expected_terms)
        })

        print(f"\n{name}")
        print(
            f"Actualización incremental: "
            f"{incremental_mean:.6f} s "
            f"(std: {incremental_std:.6f})"
        )
        print(
            f"Reconstrucción completa: "
            f"{rebuild_mean:.6f} s "
            f"(std: {rebuild_std:.6f})"
        )
        print(f"Términos afectados: {len(expected_terms)}")

    print("\nVerificación incremental: OK en las tres estructuras")

    RESULTS_PATH.mkdir(parents=True, exist_ok=True)

    output_path = RESULTS_PATH / "index_incremental.csv"

    with output_path.open("w", encoding="utf-8") as file:
        file.write(
            "structure,incremental_mean_s,incremental_std_s,"
            "rebuild_mean_s,rebuild_std_s,affected_terms\n"
        )

        for result in results:
            file.write(
                f"{result['structure']},"
                f"{result['incremental_mean_s']:.6f},"
                f"{result['incremental_std_s']:.6f},"
                f"{result['rebuild_mean_s']:.6f},"
                f"{result['rebuild_std_s']:.6f},"
                f"{result['affected_terms']}\n"
            )

    print(f"\nResultados guardados en: {output_path}")



def directory_size(path):
    if not path.exists():
        return 0

    return sum(
        file.stat().st_size
        for file in path.rglob("*")
        if file.is_file()
    )


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

    for name in STRUCTURES:
        path = INDEX_BENCHMARK_PATH / name

        size_mb = directory_size(path) / (1024 * 1024)
        files = count_files(path)
        directories = count_directories(path)

        print(
            f"{name:12} | "
            f"{size_mb:.2f} MB | "
            f"{files} archivos | "
            f"{directories} directorios"
        )


def main():
    books = load_books()

    print("========================================")
    print("    INVERTED INDEX BENCHMARK - PYTHON")
    print("========================================")
    print(f"Libros cargados: {len(books)}")
    print(f"Repeticiones: {REPETITIONS}")

    if not books:
        print("No se encontraron libros en sample_data.")
        return

    benchmark_indexing(books)

    build_all_indexes(books)

    benchmark_queries()
    verify_queries(books, "FULL BUILD")

    benchmark_incremental(books)

    build_all_indexes(books)

    verify_queries(books, "AFTER REBUILD")
    benchmark_storage()

    print("\n========================================")
    print("          BENCHMARK COMPLETADO")
    print("========================================")


if __name__ == "__main__":
    main()
