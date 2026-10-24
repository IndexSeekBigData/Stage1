
#include "inverted_index.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void memory_error(void) {
    fprintf(stderr, "Error reservando memoria\n");
    exit(EXIT_FAILURE);
}

static char *copy_string(const char *text) {
    size_t length = strlen(text);

    char *copy = malloc(length + 1);

    if (!copy) {
        memory_error();
    }

    memcpy(copy, text, length + 1);

    return copy;
}

static void ensure_index_capacity(InvertedIndex *index) {
    if (index->count < index->capacity) {
        return;
    }

    size_t new_capacity = index->capacity == 0
        ? 128
        : index->capacity * 2;

    IndexEntry *entries = realloc(
        index->entries,
        new_capacity * sizeof(IndexEntry)
    );

    if (!entries) {
        memory_error();
    }

    index->entries = entries;
    index->capacity = new_capacity;
}

static void ensure_posting_capacity(PostingList *postings) {
    if (postings->count < postings->capacity) {
        return;
    }

    size_t new_capacity = postings->capacity == 0
        ? 4
        : postings->capacity * 2;

    int *book_ids = realloc(
        postings->book_ids,
        new_capacity * sizeof(int)
    );

    if (!book_ids) {
        memory_error();
    }

    postings->book_ids = book_ids;
    postings->capacity = new_capacity;
}

static IndexEntry *find_entry(
    InvertedIndex *index,
    const char *term
) {
    size_t left = 0;
    size_t right = index->count;

    while (left < right) {
        size_t middle = left + (right - left) / 2;

        int comparison = strcmp(
            index->entries[middle].term,
            term
        );

        if (comparison == 0) {
            return &index->entries[middle];
        }

        if (comparison < 0) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }

    ensure_index_capacity(index);

    if (left < index->count) {
        memmove(
            &index->entries[left + 1],
            &index->entries[left],
            (index->count - left) * sizeof(IndexEntry)
        );
    }

    IndexEntry *entry = &index->entries[left];

    entry->term = copy_string(term);
    entry->postings.book_ids = NULL;
    entry->postings.count = 0;
    entry->postings.capacity = 0;

    index->count++;

    return entry;
}

static void add_posting(
    PostingList *postings,
    int book_id
) {
    for (size_t i = 0; i < postings->count; i++) {
        if (postings->book_ids[i] == book_id) {
            return;
        }
    }

    ensure_posting_capacity(postings);

    postings->book_ids[postings->count++] = book_id;
}

static int is_ascii_letter(unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z');
}

static int is_word_character(unsigned char character) {
    return isalnum(character) || character == '_';
}

InvertedIndex *create_index(void) {
    InvertedIndex *index = malloc(sizeof(InvertedIndex));

    if (!index) {
        memory_error();
    }

    index->entries = NULL;
    index->count = 0;
    index->capacity = 0;

    return index;
}

void add_document(
    InvertedIndex *index,
    int book_id,
    const char *text
) {
    if (!index || !text) {
        return;
    }

    size_t length = strlen(text);

    char *token = malloc(length + 1);

    if (!token) {
        memory_error();
    }

    size_t i = 0;

    while (i < length) {
        if (!is_ascii_letter((unsigned char)text[i])) {
            i++;
            continue;
        }

        size_t start = i;

        while (i < length &&
               is_ascii_letter((unsigned char)text[i])) {
            i++;
        }

        size_t end = i;
        size_t token_length = end - start;

        int valid_start =
            start == 0 ||
            !is_word_character((unsigned char)text[start - 1]);

        int valid_end =
            end == length ||
            !is_word_character((unsigned char)text[end]);

        if (token_length < 3 || !valid_start || !valid_end) {
            continue;
        }

        for (size_t j = 0; j < token_length; j++) {
            token[j] = (char)tolower(
                (unsigned char)text[start + j]
            );
        }

        token[token_length] = '\0';

        IndexEntry *entry = find_entry(index, token);

        add_posting(&entry->postings, book_id);
    }

    free(token);
}

const PostingList *search_term(
    const InvertedIndex *index,
    const char *term
) {
    if (!index || !term) {
        return NULL;
    }

    size_t length = strlen(term);

    char *normalized = malloc(length + 1);

    if (!normalized) {
        memory_error();
    }

    for (size_t i = 0; i < length; i++) {
        normalized[i] = (char)tolower(
            (unsigned char)term[i]
        );
    }

    normalized[length] = '\0';

    size_t left = 0;
    size_t right = index->count;

    while (left < right) {
        size_t middle = left + (right - left) / 2;

        int comparison = strcmp(
            index->entries[middle].term,
            normalized
        );

        if (comparison == 0) {
            free(normalized);
            return &index->entries[middle].postings;
        }

        if (comparison < 0) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }

    free(normalized);

    return NULL;
}

void free_index(InvertedIndex *index) {
    if (!index) {
        return;
    }

    for (size_t i = 0; i < index->count; i++) {
        free(index->entries[i].term);
        free(index->entries[i].postings.book_ids);
    }

    free(index->entries);
    free(index);
}
