#ifndef BOOK_LOADER_H
#define BOOK_LOADER_H

#include "inverted_index.h"

Book *load_books(const char *directory, int *count);

char *read_book_text(const Book *book);

void free_books(Book *books, int count);

#endif
