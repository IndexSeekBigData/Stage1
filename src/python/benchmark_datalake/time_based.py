from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
BENCHMARK_PATH = PROJECT_ROOT / "datalake_benchmark"


def save_book_time_based(book_id, header, body, date, hour):
    folder = (
            BENCHMARK_PATH
            / "time_based"
            / date
            / hour
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