#ifndef DATALAKE_H
#define DATALAKE_H

#include <stddef.h>

typedef struct {
    int id;
    char *body;
    char *header;
    char *date;
} Book;

typedef struct {
    const char *name;
    int (*write_book)(const char *root, const Book *book);
    int (*read_book)(const char *root, const Book *book);
} Datalake;

int time_based_write(const char *root, const Book *book);
int time_based_read(const char *root, const Book *book);

int book_based_write(const char *root, const Book *book);
int book_based_read(const char *root, const Book *book);

int range_based_write(const char *root, const Book *book);
int range_based_read(const char *root, const Book *book);

int create_directory(const char *path);
int copy_file(const char *source, const char *destination);
int file_exists(const char *path);
long long file_size(const char *path);
int delete_directory(const char *path);

#endif
