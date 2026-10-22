from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
BENCHMARK_PATH = PROJECT_ROOT / "datalake_benchmark" / "python"


def save_book_range_based(book_id, header, body):
    book_number = int(book_id)

    range_start = (int(book_id) // 25) * 25
    range_end = range_start + 24

    folder = (
            BENCHMARK_PATH
            / "range_based"
            / f"{range_start}-{range_end}"
    )

    folder.mkdir(parents=True, exist_ok=True)

    header_path = folder / f"{book_id}.header.txt"
    body_path = folder / f"{book_id}.body.txt"

    header_path.write_text(
        header,
        encoding="utf-8"
    )

    body_path.write_text(
        body,
        encoding="utf-8"
    )