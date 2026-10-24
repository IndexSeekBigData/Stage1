
import json
import shutil
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]

INDEX_PATH = (
        PROJECT_ROOT
        / "index_benchmark"
        / "python"
        / "term_files"
)


def build_index(books, tokenize):
    if INDEX_PATH.exists():
        shutil.rmtree(INDEX_PATH)

    update_index(books, tokenize)


def update_index(books, tokenize):
    INDEX_PATH.mkdir(parents=True, exist_ok=True)

    new_postings = {}

    for book_id, body in books:
        for term in set(tokenize(body)):
            new_postings.setdefault(term, set()).add(str(book_id))

    affected_terms = set(new_postings)

    for term, book_ids in new_postings.items():
        path = INDEX_PATH / f"{term}.json"

        if path.exists():
            with path.open("r", encoding="utf-8") as file:
                existing_ids = set(json.load(file))
        else:
            existing_ids = set()

        updated_ids = existing_ids | book_ids

        if updated_ids != existing_ids:
            with path.open("w", encoding="utf-8") as file:
                json.dump(
                    sorted(updated_ids, key=int),
                    file,
                    ensure_ascii=False
                )

    return affected_terms


def search_term(term):
    path = INDEX_PATH / f"{term.lower()}.json"

    if not path.exists():
        return []

    with path.open("r", encoding="utf-8") as file:
        return json.load(file)
