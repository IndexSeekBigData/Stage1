
#include "book_loader.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 4096

static void fail(const char *message) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(EXIT_FAILURE);
}

static char *copy_string(const char *text) {
    size_t length = strlen(text);

    char *copy = malloc(length + 1);

    if (!copy) {
        fail("No se pudo reservar memoria");
    }

    memcpy(copy, text, length + 1);

    return copy;
}

static int compare_books(const void *a, const void *b) {
    const Book *first = a;
    const Book *second = b;

    const char *first_name = strrchr(first->body, '\\');
    const char *second_name = strrchr(second->body, '\\');

    first_name = first_name ? first_name + 1 : first->body;
    second_name = second_name ? second_name + 1 : second->body;

    return strcmp(first_name, second_name);
}

Book *load_books(const char *directory, int *count) {
    if (!count) {
        return NULL;
    }

    *count = 0;

    char pattern[MAX_PATH_LENGTH];

    int length = snprintf(
        pattern,
        sizeof(pattern),
        "%s\\*.body.txt",
        directory
    );

    if (length < 0 || (size_t)length >= sizeof(pattern)) {
        fail("Ruta demasiado larga");
    }

    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);

    if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    Book *books = NULL;
    int capacity = 0;

    do {
        int id;
        char extra;

        if (sscanf(data.cFileName, "%d.body.txt%c", &id, &extra) != 1) {
            continue;
        }

        if (id < 0) {
            continue;
        }

        char body[MAX_PATH_LENGTH];
        char header[MAX_PATH_LENGTH];

        int body_length = snprintf(
            body,
            sizeof(body),
            "%s\\%d.body.txt",
            directory,
            id
        );

        int header_length = snprintf(
            header,
            sizeof(header),
            "%s\\%d.header.txt",
            directory,
            id
        );

        if (body_length < 0 ||
            (size_t)body_length >= sizeof(body) ||
            header_length < 0 ||
            (size_t)header_length >= sizeof(header)) {
            fail("Ruta demasiado larga");
        }

        DWORD attributes = GetFileAttributesA(header);

        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }

        if (*count >= capacity) {
            int new_capacity = capacity == 0 ? 128 : capacity * 2;

            Book *new_books = realloc(
                books,
                (size_t)new_capacity * sizeof(Book)
            );

            if (!new_books) {
                fail("No se pudo reservar memoria");
            }

            books = new_books;
            capacity = new_capacity;
        }

        books[*count].id = id;
        books[*count].body = copy_string(body);
        books[*count].header = copy_string(header);

        (*count)++;

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    qsort(books, (size_t)*count, sizeof(Book), compare_books);

    return books;
}

char *read_book_text(const Book *book) {
    if (!book) {
        return NULL;
    }

    FILE *file = fopen(book->body, "rb");

    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long size = ftell(file);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *text = malloc((size_t)size + 1);

    if (!text) {
        fclose(file);
        fail("No se pudo reservar memoria");
    }

    size_t bytes_read = fread(text, 1, (size_t)size, file);

    if (bytes_read != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }

    text[bytes_read] = '\0';

    fclose(file);

    return text;
}

void free_books(Book *books, int count) {
    if (!books) {
        return;
    }

    for (int i = 0; i < count; i++) {
        free(books[i].body);
        free(books[i].header);
    }

    free(books);
}
