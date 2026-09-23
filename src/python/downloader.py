import requests
from datetime import datetime
from pathlib import Path

START_MARKER = "*** START OF THE PROJECT GUTENBERG EBOOK"
END_MARKER = "*** END OF THE PROJECT GUTENBERG EBOOK"

book_id = 1342

url = f"https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt"

response = requests.get(url)

text = response.text

header, body_and_footer = text.split(START_MARKER, 1)
body, footer = body_and_footer.split(END_MARKER, 1)

print("HEADER:")
print(header[:1000])

print("\nBODY:")
print(body[:300])

print("\nFOOTER:")
print(footer[:300])

now = datetime.now()

date = now.strftime("%Y%m%d")
hour = now.strftime("%H")

folder = Path("datalake") / date / hour

folder.mkdir(parents=True, exist_ok=True)

header_path = folder / f"{book_id}.header.txt"
body_path = folder / f"{book_id}.body.txt"

header_path.write_text(header, encoding="utf-8")
body_path.write_text(body, encoding="utf-8")

print("Header guardado en:", header_path)
print("Body guardado en:", body_path)

print("Carpeta creada:", folder)

print("Fecha:", date)
print("Hora:", hour)