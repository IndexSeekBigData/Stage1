import os
import re
import sqlite3
from pathlib import Path


def create_metadata_db(db_path="../../datamarts/metadata.db"):

    os.makedirs(os.path.dirname(db_path), exist_ok=True)

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS books (
            book_id TEXT PRIMARY KEY,
            title TEXT,
            author TEXT,
            language TEXT,
            body_path TEXT
        )
    """)

    conn.commit()
    conn.close()


def parse_header_file(header_path):

    content = Path(header_path).read_text(encoding="utf-8", errors="ignore")

    title_match = re.search(r"Title:\s*(.+)", content, re.IGNORECASE)
    author_match = re.search(r"Author:\s*(.+)", content, re.IGNORECASE)
    language_match = re.search(r"Language:\s*(.+)", content, re.IGNORECASE)

    title = title_match.group(1).strip() if title_match else "Unknown"
    author = author_match.group(1).strip() if author_match else "Unknown"
    language = language_match.group(1).strip() if language_match else "Unknown"

    return title, author, language


def insert_book_metadata(book_id, header_path, body_path, db_path="../../datamarts/metadata.db"):

    title, author, language = parse_header_file(header_path)

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    cursor.execute("""
        INSERT OR REPLACE INTO books (book_id, title, author, language, body_path)
        VALUES (?, ?, ?, ?, ?)
    """, (str(book_id), title, author, language, str(body_path)))

    conn.commit()
    conn.close()

    print(f"[DATAMART METADATA] Procesado libro {book_id}: '{title}' por {author}")

if __name__ == "__main__":

    create_metadata_db()

    datalake_path = Path("../../datalake")
    header_files = list(datalake_path.rglob("*.header.txt"))

    if header_files:
        for header_file in header_files:
            book_id = header_file.name.split(".")[0]
            body_file = header_file.parent / f"{book_id}.body.txt"

            insert_book_metadata(book_id, header_file, body_file)
    else:
        print("No se encontraron archivos .header.txt dentro de la carpeta 'datalake/'. Ejecuta primero downloader.py.")