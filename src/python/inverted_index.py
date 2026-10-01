import json
import os
import re
from pathlib import Path


def tokenize(text):

    words = re.findall(r'\b[a-zA-Z]{3,}\b', text.lower())
    return set(words)


def load_existing_index(index_path="datamarts/inverted_index.json"):

    if os.path.exists(index_path):
        with open(index_path, "r", encoding="utf-8") as f:
            return json.load(f)
    return {}


def save_index(index_data, index_path="datamarts/inverted_index.json"):

    os.makedirs(os.path.dirname(index_path), exist_ok=True)
    with open(index_path, "w", encoding="utf-8") as f:
        json.dump(index_data, f, ensure_ascii=False, indent=2)

def mark_as_indexed(book_id):
    control_file = Path("../../control/indexed_books.txt")

    indexed_books = control_file.read_text(encoding="utf-8").splitlines()

    if str(book_id) not in indexed_books:
        with control_file.open("a", encoding="utf-8") as file:
            file.write(f"{book_id}\n")

def index_book(book_id, body_path, index_path="datamarts/inverted_index.json"):

    text = Path(body_path).read_text(encoding="utf-8", errors="ignore")

    terms = tokenize(text)

    inverted_index = load_existing_index(index_path)

    book_id_str = str(book_id)
    for term in terms:
        if term not in inverted_index:
            inverted_index[term] = []
        if book_id_str not in inverted_index[term]:
            inverted_index[term].append(book_id_str)

    save_index(inverted_index, index_path)
    mark_as_indexed(book_id)

    print(f"[INDEXER] Libro {book_id} indexado con éxito ({len(terms)} términos únicos).")

# Prueba
if __name__ == "__main__":
    datalake_path = Path("datalake")
    body_files = list(datalake_path.rglob("*.body.txt"))

    if body_files:
        for body_file in body_files:
            book_id = body_file.name.split(".")[0]
            index_book(book_id, body_file)
    else:
        print("No se encontraron archivos .body.txt en el Datalake.")

