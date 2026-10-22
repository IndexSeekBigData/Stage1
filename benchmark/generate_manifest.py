
import csv
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SAMPLE_DATA_PATH = PROJECT_ROOT / "sample_data"
OUTPUT_PATH = PROJECT_ROOT / "benchmark" / "corpus_manifest.csv"


def generate_manifest():
    rows = []

    for body_path in SAMPLE_DATA_PATH.glob("*.body.txt"):
        book_id = body_path.name.split(".")[0]
        header_path = SAMPLE_DATA_PATH / f"{book_id}.header.txt"

        if not header_path.is_file():
            raise FileNotFoundError(
                f"Falta el header del libro {book_id}"
            )

        header_size = header_path.stat().st_size
        body_size = body_path.stat().st_size

        rows.append({
            "book_id": book_id,
            "header_bytes": header_size,
            "body_bytes": body_size,
            "total_bytes": header_size + body_size
        })

    rows.sort(key=lambda row: int(row["book_id"]))

    if len(rows) != 100:
        raise ValueError(
            f"Se esperaban 100 libros, pero se encontraron {len(rows)}"
        )

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)

    with OUTPUT_PATH.open(
            "w",
            newline="",
            encoding="utf-8"
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=[
                "book_id",
                "header_bytes",
                "body_bytes",
                "total_bytes"
            ]
        )

        writer.writeheader()
        writer.writerows(rows)

    total_bytes = sum(row["total_bytes"] for row in rows)

    print("========== CORPUS MANIFEST ==========")
    print(f"Libros registrados: {len(rows)}")
    print(f"Tamaño total: {total_bytes / (1024 * 1024):.2f} MiB")
    print(f"Archivo generado: {OUTPUT_PATH}")


if __name__ == "__main__":
    generate_manifest()
