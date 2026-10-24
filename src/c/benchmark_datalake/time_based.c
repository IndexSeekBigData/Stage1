
#include "datalake.h"

#include <stdio.h>

static int build_paths(
    const char *root,
    const Book *book,
    char *date_directory,
    char *hour_directory,
    char *body_path,
    char *header_path
) {
    if (!root || !book || !book->date) {
        return 0;
    }

    int n = snprintf(
        date_directory,
        4096,
        "%s\\%s",
        root,
        book->date
    );

    if (n < 0 || n >= 4096) {
        return 0;
    }

    n = snprintf(
        hour_directory,
        4096,
        "%s\\22",
        date_directory
    );

    if (n < 0 || n >= 4096) {
        return 0;
    }

    n = snprintf(
        body_path,
        4096,
        "%s\\%d.body.txt",
        hour_directory,
        book->id
    );

    if (n < 0 || n >= 4096) {
        return 0;
    }

    n = snprintf(
        header_path,
        4096,
        "%s\\%d.header.txt",
        hour_directory,
        book->id
    );

    return n >= 0 && n < 4096;
}

int time_based_write(const char *root, const Book *book) {
    char date_directory[4096];
    char hour_directory[4096];
    char body_path[4096];
    char header_path[4096];

    if (!book || !book->body || !book->header) {
        return 0;
    }

    if (!build_paths(
            root,
            book,
            date_directory,
            hour_directory,
            body_path,
            header_path
        )) {
        return 0;
    }

    if (!create_directory(root) ||
        !create_directory(date_directory) ||
        !create_directory(hour_directory)) {
        return 0;
    }

    return copy_file(book->body, body_path) &&
           copy_file(book->header, header_path);
}

int time_based_read(const char *root, const Book *book) {
    char date_directory[4096];
    char hour_directory[4096];
    char body_path[4096];
    char header_path[4096];

    if (!build_paths(
            root,
            book,
            date_directory,
            hour_directory,
            body_path,
            header_path
        )) {
        return 0;
    }

    return file_exists(body_path) &&
           file_exists(header_path);
}
