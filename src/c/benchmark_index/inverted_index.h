
#ifndef INVERTED_INDEX_H
#define INVERTED_INDEX_H

#include <stddef.h>

typedef struct {
    int id;
    char *body;
    char *header;
} Book;

typedef struct {
    int *book_ids;
    size_t count;
    size_t capacity;
} PostingList;

typedef struct {
    char *term;
    PostingList postings;
} IndexEntry;

typedef struct {
    IndexEntry *entries;
    size_t count;
    size_t capacity;
} InvertedIndex;

InvertedIndex *create_index(void);

void add_document(
    InvertedIndex *index,
    int book_id,
    const char *text
);

const PostingList *search_term(
    const InvertedIndex *index,
    const char *term
);

void free_index(InvertedIndex *index);

#endif
