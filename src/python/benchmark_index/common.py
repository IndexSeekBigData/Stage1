import re


def tokenize(text):
    words = re.findall(
        r"\b[a-zA-Z]{3,}\b",
        text.lower()
    )

    return set(words)