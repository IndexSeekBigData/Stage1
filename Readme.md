# InverSeek

**A Big Data Search Engine Based on Inverted Indexes**

InverSeek is an academic Big Data project focused on designing, implementing, and evaluating a document retrieval system using books from Project Gutenberg.

The project explores different strategies for storing, indexing, and retrieving documents, with implementations in **Python, Java, and C**.

A major objective is to compare the performance of different data structures and programming languages through reproducible benchmarks.

## 1. Project Overview

InverSeek implements the main components of a document search engine:

- **Crawler / Downloader:** Retrieves books from Project Gutenberg.
- **Data Lake:** Stores downloaded documents and their metadata.
- **Indexer:** Processes documents and builds inverted indexes.
- **Search Engine:** Retrieves documents containing specified search terms.
- **Control System:** Tracks downloaded and indexed books.
- **Benchmarking System:** Evaluates performance, scalability, storage requirements, and incremental operations.

The project combines practical search engine development with experimental performance analysis.

## 2. System Architecture

The general processing pipeline is:

```text
                  Project Gutenberg
                          |
                          v
                 Crawler / Downloader
                          |
                          v
                      Data Lake
                          |
                 +--------+--------+
                 |                 |
                 v                 v
              Headers           Book Bodies
                 |                 |
                 v                 v
              Metadata           Indexer
                                   |
                                   v
                             Inverted Index
                                   |
                                   v
                              Search Engine
                                   |
                                   v
                              Search Results
```

The system separates document acquisition, storage, indexing, and retrieval to allow independent evaluation of its components.

## 3. Technologies

The project includes implementations in three programming languages:

| Language | Purpose |
|---|---|
| Python | Search engine implementation, data processing, benchmarking, and visualization |
| Java | Alternative implementation for performance comparison |
| C | Low-level implementation for performance comparison |

Additional technologies include:

- CSV for experimental results.
- JSON and filesystem-based storage structures.
- Matplotlib for benchmark visualizations.
- Git for version control.

## 4. Dataset

The experimental dataset consists of **100 books from Project Gutenberg**.

Each book is represented by two files:

```text
<book_id>.header.txt
<book_id>.body.txt
```

The header contains book-related information, while the body contains the document text.

### Corpus Manifest

The script `benchmark/generate_manifest.py` generates a manifest describing the experimental corpus.

The resulting file, `benchmark/corpus_manifest.csv`, contains:

| Field | Description |
|---|---|
| `book_id` | Unique book identifier |
| `header_bytes` | Header file size in bytes |
| `body_bytes` | Body file size in bytes |
| `total_bytes` | Combined size in bytes |

The script also verifies that the corpus contains exactly 100 books and that each body has a corresponding header.

To generate the manifest, run the following command from the project root:

```bash
python benchmark/generate_manifest.py
```

## 5. Data Lake

The data lake is responsible for organizing and storing downloaded books.

Three storage layouts have been implemented.

### 5.1 Time-Based Storage

Documents are organized according to a time-based directory structure.

This strategy groups documents by temporal information.

### 5.2 Book-Based Storage

Documents are organized into directories associated with individual books.

This strategy provides direct organization by document identifier.

### 5.3 Range-Based Storage

Documents are organized into directories based on ranges of book identifiers.

This strategy groups multiple documents within the same directory.

### Data Lake Operations

The implementations support the following evaluated operations:

- Writing documents.
- Looking up stored books.
- Incrementally inserting new books.
- Detecting downloaded books pending indexing.
- Measuring storage requirements.
- Recovering after an interrupted operation.
- Evaluating scalability with different corpus sizes.

## 6. Inverted Index

An inverted index maps search terms to the documents containing them.

For example:

```text
"science"  -> [12, 25, 41]
"history"  -> [8, 12, 37]
"computer" -> [25, 41, 63]
```

Instead of scanning every document for each query, the search engine consults the corresponding postings.

Three inverted-index structures have been implemented.

### 6.1 Monolithic Index

The complete inverted index is stored in a single file.

**Advantages:**
- Simple storage organization.
- Efficient full-index construction in the measured experiments.
- Only one index file.

**Trade-off:**
- Queries may require accessing a large index representation.

### 6.2 Term-Based Files

Each indexed term is stored in an individual file.

**Advantages:**
- Direct access to individual term postings.
- Fast term lookup in the measured experiments.

**Trade-off:**
- Produces a large number of files.

### 6.3 Hierarchical Index

Term files are organized into a hierarchical directory structure.

**Advantages:**
- Organizes term files into directories.
- Supports direct access to individual term postings.

**Trade-off:**
- Introduces additional filesystem organization.

## 7. Search Functionality

The search component uses the inverted index to retrieve documents associated with a query term.

The benchmarking suite evaluates common terms, less frequent terms, and nonexistent terms.

The tested queries include:

```text
the
and
love
man
woman
time
house
book
zzzznotfound
```

Query correctness is verified against expected document sets.

The benchmark suite also checks correctness after index reconstruction.

## 8. Benchmarking Methodology

The project evaluates the three programming languages and the three structures implemented for each component.

This produces:

- **9 data lake configurations:** 3 layouts × 3 languages.
- **9 inverted-index configurations:** 3 structures × 3 languages.

### 8.1 Data Lake Benchmarks

The main data lake experiments use:

- 100 books.
- 5 benchmark repetitions.
- Three storage layouts.
- Three programming languages.

The evaluated metrics include:

| Metric | Description |
|---|---|
| Write time | Time required to store the corpus |
| Throughput | Number of books written per second |
| Lookup time | Average document lookup latency |
| Incremental insertion | Time required to add 10 books |
| Pending discovery | Time required to identify books awaiting indexing |
| Storage | Disk space and filesystem organization |
| Recovery | Correctness and recovery time after interruption |
| Scalability | Performance with increasing corpus sizes |

Scalability experiments use:

**10, 25, 50, and 100 books.**

### 8.2 Inverted Index Benchmarks

The main inverted-index experiments use:

- 100 books.
- 3 benchmark repetitions.
- Three index structures.
- Three programming languages.

The evaluated metrics include:

| Metric | Description |
|---|---|
| Full build | Time required to construct the complete index |
| Query latency | Average time required to execute a query |
| Incremental update | Time required to index 10 additional books |
| Full rebuild | Time required to reconstruct the index |
| Storage | Disk space, files, and directories |
| Correctness | Verification of query results and incremental updates |

Incremental experiments begin with 90 books and incorporate 10 additional books.

## 9. Experimental Results

The project includes automated comparisons across all implementations.

### 9.1 Data Lake Results

The following table presents the average write time for 100 books.

**Lower values indicate better performance.**

| Storage Layout | Python (s) | Java (s) | C (s) |
|---|---:|---:|---:|
| Time-Based | 0.198737 | 0.187218 | **0.105407** |
| Book-Based | 0.191481 | 0.162617 | **0.113575** |
| Range-Based | 0.276483 | **0.161645** | 0.177346 |

![Data Lake Write Performance](docs/figures/datalake_write.png)

### 9.2 Inverted Index Results

The following table presents the average full-index construction time for 100 books.

| Index Structure | Python (s) | Java (s) | C (s) |
|---|---:|---:|---:|
| Monolithic | 4.449514 | **1.416649** | 3.170840 |
| Term Files | 62.729803 | **25.773413** | 71.786624 |
| Hierarchical | 70.947523 | **30.721023** | 75.301243 |

![Inverted Index Build Performance](docs/figures/index_build.png)

### 9.3 Query Performance

Average query latency:

| Index Structure | Python (ms) | Java (ms) | C (ms) |
|---|---:|---:|---:|
| Monolithic | **85.085707** | 96.005345 | 103.343015 |
| Term Files | 0.124572 | 0.063292 | **0.048613** |
| Hierarchical | 0.121877 | 0.064876 | **0.048258** |

![Inverted Index Query Performance](docs/figures/index_query.png)

### 9.4 Main Observations

The experimental results reveal several performance trade-offs:

- C achieves the lowest measured data lake write time for the time-based and book-based layouts.
- Java achieves the lowest measured construction times for all three inverted-index structures.
- The monolithic index offers substantially faster full-index construction than the file-based alternatives.
- Term-based and hierarchical indexes provide substantially lower query latency than the monolithic index.
- Incremental updates can reduce processing time compared with full reconstruction, although this depends on the implementation and index structure.
- Storage organization affects the number of generated files and directories.

These observations describe the measured implementations rather than universal performance characteristics of the programming languages.

### 9.5 Experimental Limitations

Some experimental configurations differ in corpus storage size, indexed vocabulary, or document identifiers across implementations.

In particular, the data lake incremental experiments do not use identical new-book identifier sets in all three languages.

Consequently, cross-language comparisons should be interpreted as descriptive measurements of the implemented systems rather than perfectly controlled language-only comparisons.

The complete experimental results, including additional metrics and scalability tests, are available here:

**[Complete Benchmark Results](benchmark/RESULTADOS.md)**

## 10. Benchmark Visualizations

All benchmark figures are generated automatically and stored in:

```text
docs/figures/
```

The visualizations cover:

- Data lake writing.
- Data lake lookup.
- Data lake incremental insertion.
- Pending-book discovery.
- Data lake storage.
- Data lake scalability.
- Inverted-index construction.
- Query performance.
- Incremental indexing.
- Full reconstruction.
- Index storage.

### Generate Comparisons

Install Matplotlib:

```bash
python -m pip install matplotlib
```

Then run:

```bash
python benchmark/generate_comparisons.py
```

The script reads the existing benchmark outputs and generates:

```text
benchmark/
├── comparative_results.csv
├── comparative_scalability.csv
└── RESULTADOS.md

docs/
└── figures/
    ├── datalake_write.png
    ├── datalake_lookup.png
    ├── datalake_incremental.png
    ├── datalake_pending.png
    ├── datalake_storage.png
    ├── datalake_scalability_write.png
    ├── datalake_scalability_lookup.png
    ├── index_build.png
    ├── index_query.png
    ├── index_incremental.png
    ├── index_rebuild.png
    └── index_storage.png
```

The comparison script uses previously generated benchmark results and does not rerun the original experiments.

## 11. Reproducibility

Reproducibility is an important objective of the project.

The repository includes:

- A fixed experimental corpus of 100 books.
- A corpus manifest containing document identifiers and file sizes.
- Raw benchmark outputs.
- CSV files containing experimental measurements.
- Automated scripts for generating comparative results.
- Benchmark visualizations.
- Correctness checks for index construction and updates.
- Recovery tests for interrupted data lake operations.

### Regenerating the Comparative Results

From the project root:

```bash
python benchmark/generate_manifest.py
python benchmark/generate_comparisons.py
```

The first command regenerates the corpus manifest.

The second command regenerates the comparative tables and figures using the stored benchmark outputs.

Reproducing the original timing measurements additionally requires executing the corresponding language-specific benchmark implementations.

## 12. Project Structure

The main project components are organized as follows:

```text
InverSeek/
│
├── benchmark/
│   ├── generate_manifest.py
│   ├── generate_comparisons.py
│   ├── corpus_manifest.csv
│   ├── comparative_results.csv
│   ├── comparative_scalability.csv
│   ├── RESULTADOS.md
│   └── ...
│
├── control/
│
├── docs/
│   └── figures/
│
├── sample_data/
│   ├── <book_id>.header.txt
│   └── <book_id>.body.txt
│
├── src/
│   ├── python/
│   ├── java/
│   └── c/
│
└── README.md
```

This diagram summarizes the main components rather than listing every source file.

## 13. Project Objectives

The main objectives of InverSeek are:

1. Implement a document acquisition and storage pipeline.
2. Develop multiple data lake organization strategies.
3. Build and query inverted indexes.
4. Support incremental processing of newly downloaded documents.
5. Validate the correctness of storage and indexing operations.
6. Implement comparable functionality in Python, Java, and C.
7. Evaluate execution time, storage requirements, and scalability.
8. Document experimental results and implementation trade-offs.


## 14. Data Source

The experimental books originate from **Project Gutenberg**, a digital library providing access to public-domain and other freely available electronic books.

Book availability and usage conditions are governed by Project Gutenberg's applicable terms and the legal status of individual works.

---

**InverSeek — Big Data Search Engine Project**