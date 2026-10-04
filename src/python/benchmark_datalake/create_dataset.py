import requests
from pathlib import Path


START_MARKER = "*** START OF THE PROJECT GUTENBERG EBOOK"
END_MARKER = "*** END OF THE PROJECT GUTENBERG EBOOK"

DATASET_PATH = Path("../../../sample_data")

NUMBER_OF_BOOKS = 100


def download_book(book_id):
    url = (
        f"https://www.gutenberg.org/cache/epub/"
        f"{book_id}/pg{book_id}.txt"
    )

    try:
        response = requests.get(url, timeout=30)

        if response.status_code != 200:
            return False

        text = response.text

        if START_MARKER not in text or END_MARKER not in text:
            return False

        header, body_and_footer = text.split(
            START_MARKER, 1
        )

        body, _ = body_and_footer.split(
            END_MARKER, 1
        )

        DATASET_PATH.mkdir(
            parents=True,
            exist_ok=True
        )

        header_path = (
                DATASET_PATH /
                f"{book_id}.header.txt"
        )

        body_path = (
                DATASET_PATH /
                f"{book_id}.body.txt"
        )

        header_path.write_text(
            header,
            encoding="utf-8"
        )

        body_path.write_text(
            body,
            encoding="utf-8"
        )

        return True

    except requests.RequestException:
        return False


def create_dataset():
    downloaded = 0
    book_id = 1

    while downloaded < NUMBER_OF_BOOKS:

        print(
            f"Probando libro {book_id}...",
            end=" "
        )

        if download_book(book_id):
            downloaded += 1

            print(
                f"OK ({downloaded}/{NUMBER_OF_BOOKS})"
            )
        else:
            print("No disponible")

        book_id += 1

    print(
        f"\nDataset creado con "
        f"{downloaded} libros."
    )


if __name__ == "__main__":
    create_dataset()