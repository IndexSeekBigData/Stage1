import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]

INDEX_PATH = (
        PROJECT_ROOT
        / "index_benchmark"
        / "python"
        / "monolithic"
        / "inverted_index.json"
)


def build_index(books, tokenize):
    inverted_index = {}

    for book_id, body in books:
        for term in tokenize(body):
            inverted_index.setdefault(term, set()).add(str(book_id))

    save_index(inverted_index)


def save_index(inverted_index):
    INDEX_PATH.parent.mkdir(parents=True, exist_ok=True)

    serializable = {
        term: sorted(book_ids, key=int)
        for term, book_ids in inverted_index.items()
    }

    with INDEX_PATH.open("w", encoding="utf-8") as file:
        json.dump(serializable, file, ensure_ascii=False)


def update_index(books, tokenize):
    if INDEX_PATH.exists():
        with INDEX_PATH.open("r", encoding="utf-8") as file:
            existing_index = json.load(file)
    else:
        existing_index = {}

    inverted_index = {
        term: set(book_ids)
        for term, book_ids in existing_index.items()
    }

    affected_terms = set()

    for book_id, body in books:
        for term in tokenize(body):
            inverted_index.setdefault(term, set()).add(str(book_id))
            affected_terms.add(term)

    save_index(inverted_index)

    return affected_terms


def search_term(term):
    if not INDEX_PATH.exists():
        return []

    with INDEX_PATH.open("r", encoding="utf-8") as file:
        index = json.load(file)

    return index.get(term.lower(), [])
