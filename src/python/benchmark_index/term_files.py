import json
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]

INDEX_PATH = (
        PROJECT_ROOT
        / "index_benchmark"
        / "term_files"
)


def build_index(books, tokenize):
    inverted_index = {}

    for book_id, body in books:
        terms = tokenize(body)

        for term in terms:
            if term not in inverted_index:
                inverted_index[term] = []

            inverted_index[term].append(str(book_id))

    INDEX_PATH.mkdir(
        parents=True,
        exist_ok=True
    )

    for term, book_ids in inverted_index.items():
        path = INDEX_PATH / f"{term}.json"

        with path.open(
                "w",
                encoding="utf-8"
        ) as file:
            json.dump(
                book_ids,
                file,
                ensure_ascii=False
            )


def search_term(term):
    path = INDEX_PATH / f"{term.lower()}.json"

    if not path.exists():
        return []

    with path.open(
            "r",
            encoding="utf-8"
    ) as file:
        return json.load(file)