import requests

START_MARKER = "*** START OF THE PROJECT GUTENBERG EBOOK"
END_MARKER = "*** END OF THE PROJECT GUTENBERG EBOOK"

book_id = 1342

url = f"https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt"

response = requests.get(url)

text = response.text

header, body_and_footer = text.split(START_MARKER, 1)
body, footer = body_and_footer.split(END_MARKER, 1)

print("HEADER:")
print(header[:2000])

print("\nBODY:")
print(body[:6000])

print("\nFOOTER:")
print(footer[:300])