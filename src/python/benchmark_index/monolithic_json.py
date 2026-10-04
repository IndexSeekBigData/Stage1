import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]

INDEX_PATH = (
        PROJECT_ROOT
        / "index_benchmark"
        / "monolithic"
        / "inverted_index.json"
)


def build_index(books, tokenize):
    inverted_index = {}

    for book_id, body in books:
        terms = tokenize(body)

        for term in terms:
            if term not in inverted_index:
                inverted_index[term] = []

            inverted_index[term].append(str(book_id))

    INDEX_PATH.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    with INDEX_PATH.open(
            "w",
            encoding="utf-8"
    ) as file:
        json.dump(
            inverted_index,
            file,
            ensure_ascii=False
        )


def search_term(term):
    if not INDEX_PATH.exists():
        return []

    with INDEX_PATH.open(
            "r",
            encoding="utf-8"
    ) as file:
        index = json.load(file)

    return index.get(term.lower(), [])