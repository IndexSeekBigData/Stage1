#include "datalake.h"

#include <stdio.h>

static int build_paths(
    const char *root,
    const Book *book,
    char *directory,
    char *body_path,
    char *header_path
) {
    if (!root || !book) {
        return 0;
    }

    int n = snprintf(directory, 4096, "%s\\%d", root, book->id);

    if (n < 0 || n >= 4096) {
        return 0;
    }

    n = snprintf(body_path, 4096, "%s\\%d.body.txt", directory, book->id);

    if (n < 0 || n >= 4096) {
        return 0;
    }

    n = snprintf(header_path, 4096, "%s\\%d.header.txt", directory, book->id);

    return n >= 0 && n < 4096;
}

int book_based_write(const char *root, const Book *book) {
    char directory[4096];
    char body_path[4096];
    char header_path[4096];

    if (!book || !book->body || !book->header) {
        return 0;
    }

    if (!build_paths(root, book, directory, body_path, header_path)) {
        return 0;
    }

    if (!create_directory(root) || !create_directory(directory)) {
        return 0;
    }

    return copy_file(book->body, body_path) &&
           copy_file(book->header, header_path);
}

int book_based_read(const char *root, const Book *book) {
    char directory[4096];
    char body_path[4096];
    char header_path[4096];

    if (!build_paths(root, book, directory, body_path, header_path)) {
        return 0;
    }

    return file_exists(body_path) && file_exists(header_path);
}