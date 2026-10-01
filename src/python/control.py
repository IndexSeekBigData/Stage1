from pathlib import Path
from inverted_index import index_book
from downloader import download_book

BOOK_IDS = [1342, 345, 84, 11, 1661, 2701, 23, 456]

def get_pending_books():
    downloaded_path = Path("../../control/downloaded_books.txt")
    indexed_path = Path("../../control/indexed_books.txt")

    downloaded = set(
        downloaded_path.read_text(encoding="utf-8").splitlines()
    )

    indexed = set(
        indexed_path.read_text(encoding="utf-8").splitlines()
    )

    pending = downloaded - indexed

    return pending


def process_pending_book(book_id):
    datalake_path = Path("../../datalake")

    body_files = list(
        datalake_path.rglob(f"{book_id}.body.txt")
    )

    if body_files:
        body_path = body_files[0]
        index_book(book_id, body_path)
    else:
        print(f"No se encontró el body del libro {book_id}")

def is_book_downloaded(book_id):
    downloaded_path = Path("../../control/downloaded_books.txt")

    downloaded = set(
        downloaded_path.read_text(encoding="utf-8").splitlines()
    )

    return str(book_id) in downloaded

def download_new_book():
    for book_id in BOOK_IDS:
        if not is_book_downloaded(book_id):
            download_book(book_id)
            return

    print("No quedan libros nuevos para descargar.")

pending_books = get_pending_books()

if pending_books:
    book_id = next(iter(pending_books))
    process_pending_book(book_id)
else:
    download_new_book()



