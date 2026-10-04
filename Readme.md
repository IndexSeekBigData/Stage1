# InverSeek - Stage 1

InverSeek is a search engine project based on an inverted index. This first stage focuses on building the data layer required to download, store, organize and index books obtained from Project Gutenberg.

## Project Structure

```text
InverSeek/
├── benchmark/
├── control/
│   ├── downloaded_books.txt
│   └── indexed_books.txt
├── datalake/
├── datamarts/
│   ├── metadata.db
│   └── inverted_index.json
├── sample_data/
├── src/
│   ├── c/
│   ├── java/
│   └── python/
│       ├── benchmark_datalake/
│       ├── benchmark_index/
│       ├── control.py
│       ├── downloader.py
│       ├── inverted_index.py
│       └── metadata.py
└── README.md
```

## Data Source

The dataset is obtained from Project Gutenberg.

Books are downloaded using their Gutenberg identifier from:

```text
https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt
```

Each downloaded book is separated into two parts:

- Header: contains metadata such as title, author and language.
- Body: contains the actual text of the book.

## Datalake

Downloaded books are stored in the Datalake as:

```text
datalake/
└── YYYYMMDD/
    └── HH/
        ├── BOOK_ID.header.txt
        └── BOOK_ID.body.txt
```

Three different Datalake organization strategies were implemented for benchmarking:

- Time-based organization.
- Book-based organization.
- Range-based organization.

The benchmark evaluates:

- Write performance and throughput.
- Lookup performance.
- Incremental processing.
- Storage overhead.
- Recovery behavior.

## Datamarts

Two main Datamarts are generated.

### Metadata

Book metadata is extracted from the Gutenberg header and stored in SQLite.

The main information stored is:

- Book ID.
- Title.
- Author.
- Language.
- Body path.

### Inverted Index

The body of each book is tokenized and used to construct an inverted index.

The index associates each term with the books in which the term appears.

Example:

```json
{
  "data": ["11", "84", "1342"],
  "search": ["84", "1661"]
}
```

Three storage strategies are evaluated:

- Monolithic JSON index.
- One file per term.
- Hierarchical term files organized by initial character.

All implementations use the same dataset and tokenization process to ensure comparable benchmark results.

## Control Layer

The Control Layer keeps track of downloaded and indexed books using:

```text
control/downloaded_books.txt
control/indexed_books.txt
```

If a downloaded book has not yet been indexed, it is processed by the indexer.

If there are no pending books, the system downloads a new book from Project Gutenberg.

## Benchmark Dataset

A fixed sample dataset is used to perform the benchmarks.

Using the same books for every implementation allows the different storage strategies to be compared under equivalent conditions.

## Requirements

The Python implementation requires Python 3 and the `requests` library.

Install the dependency with:

```bash
pip install requests
```

## Execution

To execute the main components individually:

```bash
python src/python/downloader.py
python src/python/metadata.py
python src/python/inverted_index.py
python src/python/control.py
```

To run the Datalake benchmark:

```bash
python src/python/benchmark_datalake/benchmark.py
```

To run the inverted index benchmark:

```bash
python src/python/benchmark_index/benchmark.py
```

## Current Implementation

The current implementation includes:

- Project Gutenberg book downloader.
- Header and body extraction.
- Datalake storage.
- SQLite metadata Datamart.
- Inverted index.
- Control Layer.
- Datalake organization benchmarks.
- Inverted index storage benchmarks.

## Authors

- Alejandro Hernández De León
- Diego Ruiz Rodríguez
- Julen Mendoza Borrero

## Stage 1

This repository corresponds to Stage 1: Building the Data Layer of the Big Data search engine project.