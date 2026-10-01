import requests
from datetime import datetime
from pathlib import Path

START_MARKER = "*** START OF THE PROJECT GUTENBERG EBOOK"
END_MARKER = "*** END OF THE PROJECT GUTENBERG EBOOK"


def download_book(book_id):

    url = f"https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt"

    try:
        response = requests.get(url)
        response.raise_for_status()
    except requests.RequestException as error:
        print("Error al descargar el libro:", error)
        return

    text = response.text

    header, body_and_footer = text.split(START_MARKER, 1)
    body, footer = body_and_footer.split(END_MARKER, 1)

    now = datetime.now()

    date = now.strftime("%Y%m%d")
    hour = now.strftime("%H")

    folder = Path("datalake") / date / hour
    folder.mkdir(parents=True, exist_ok=True)

    header_path = folder / f"{book_id}.header.txt"
    body_path = folder / f"{book_id}.body.txt"

    header_path.write_text(header, encoding="utf-8")
    body_path.write_text(body, encoding="utf-8")

    mark_as_downloaded(book_id)

    print("Header guardado en:", header_path)
    print("Body guardado en:", body_path)



def mark_as_downloaded(book_id):
    control_file = Path("../../control/downloaded_books.txt")

    downloaded_books = control_file.read_text(encoding="utf-8").splitlines()

    if str(book_id) not in downloaded_books:
        with control_file.open("a", encoding="utf-8") as file:
            file.write(f"{book_id}\n")



download_book(345)
download_book(1342)
download_book(84)
download_book(999999999)
