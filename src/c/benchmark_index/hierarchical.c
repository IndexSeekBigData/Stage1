
#include "hierarchical.h"
#include "cJSON.h"

#include <windows.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 4096

static int compare_ids(const void *a, const void *b) {
    int first = *(const int *)a;
    int second = *(const int *)b;
    return (first > second) - (first < second);
}

static int create_directory(const char *path) {
    if (CreateDirectoryA(path, NULL)) {
        return 1;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return 0;
    }

    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static int build_paths(
    const char *directory,
    const char *term,
    char *folder,
    size_t folder_capacity,
    char *path,
    size_t path_capacity
) {
    if (!term || !term[0]) {
        return 0;
    }

    unsigned char first = (unsigned char)term[0];

    if (first < 'a' || first > 'z') {
        return 0;
    }

    int length = snprintf(
        folder,
        folder_capacity,
        "%s\\%c",
        directory,
        first
    );

    if (length < 0 || (size_t)length >= folder_capacity) {
        return 0;
    }

    length = snprintf(
        path,
        path_capacity,
        "%s\\%s.json",
        folder,
        term
    );

    return length >= 0 && (size_t)length < path_capacity;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");

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

    char *content = malloc((size_t)size + 1);

    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(content, 1, (size_t)size, file);

    fclose(file);

    if (read_count != (size_t)size) {
        free(content);
        return NULL;
    }

    content[size] = '\0';

    return content;
}

static cJSON *load_json(const char *path) {
    char *content = read_file(path);

    if (!content) {
        return NULL;
    }

    cJSON *json = cJSON_Parse(content);

    free(content);

    return json;
}

static int write_json(const cJSON *json, const char *path) {
    char *content = cJSON_PrintUnformatted(json);

    if (!content) {
        return 0;
    }

    FILE *file = fopen(path, "wb");

    if (!file) {
        cJSON_free(content);
        return 0;
    }

    size_t length = strlen(content);

    int success = fwrite(content, 1, length, file) == length;

    if (fclose(file) != 0) {
        success = 0;
    }

    cJSON_free(content);

    return success;
}

static PostingList *load_postings(const char *path) {
    cJSON *array = load_json(path);

    if (!cJSON_IsArray(array)) {
        cJSON_Delete(array);
        return NULL;
    }

    PostingList *result = calloc(1, sizeof(PostingList));

    if (!result) {
        cJSON_Delete(array);
        return NULL;
    }

    size_t count = (size_t)cJSON_GetArraySize(array);

    if (count > 0) {
        result->book_ids = malloc(count * sizeof(int));

        if (!result->book_ids) {
            free(result);
            cJSON_Delete(array);
            return NULL;
        }
    }

    result->capacity = count;

    cJSON *item = NULL;

    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            free(result->book_ids);
            free(result);
            cJSON_Delete(array);
            return NULL;
        }

        char *end;
        long value = strtol(item->valuestring, &end, 10);

        if (*end != '\0' || value < 0 || value > INT_MAX) {
            free(result->book_ids);
            free(result);
            cJSON_Delete(array);
            return NULL;
        }

        result->book_ids[result->count++] = (int)value;
    }

    cJSON_Delete(array);

    return result;
}

static int save_postings(
    const char *path,
    const int *ids,
    size_t count
) {
    int *sorted = NULL;

    if (count > 0) {
        sorted = malloc(count * sizeof(int));

        if (!sorted) {
            return 0;
        }

        memcpy(sorted, ids, count * sizeof(int));
        qsort(sorted, count, sizeof(int), compare_ids);
    }

    cJSON *array = cJSON_CreateArray();

    if (!array) {
        free(sorted);
        return 0;
    }

    int success = 1;

    for (size_t i = 0; i < count; i++) {
        if (i > 0 && sorted[i] == sorted[i - 1]) {
            continue;
        }

        char buffer[32];

        snprintf(buffer, sizeof(buffer), "%d", sorted[i]);

        cJSON *item = cJSON_CreateString(buffer);

        if (!item) {
            success = 0;
            break;
        }

        cJSON_AddItemToArray(array, item);
    }

    if (success) {
        success = write_json(array, path);
    }

    cJSON_Delete(array);
    free(sorted);

    return success;
}

static int update_postings(
    const char *path,
    const PostingList *new_postings
) {
    DWORD attributes = GetFileAttributesA(path);
    int exists = attributes != INVALID_FILE_ATTRIBUTES;

    PostingList *existing = NULL;

    if (exists) {
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
            return 0;
        }

        existing = load_postings(path);

        if (!existing) {
            return 0;
        }
    }

    size_t existing_count = existing ? existing->count : 0;
    size_t total = existing_count + new_postings->count;

    int *ids = NULL;

    if (total > 0) {
        ids = malloc(total * sizeof(int));

        if (!ids) {
            if (existing) {
                free(existing->book_ids);
                free(existing);
            }

            return 0;
        }
    }

    size_t count = 0;
    int changed = !exists;

    for (size_t i = 0; i < existing_count; i++) {
        ids[count++] = existing->book_ids[i];
    }

    for (size_t i = 0; i < new_postings->count; i++) {
        int id = new_postings->book_ids[i];
        int found = 0;

        for (size_t j = 0; j < existing_count; j++) {
            if (existing->book_ids[j] == id) {
                found = 1;
                break;
            }
        }

        if (!found) {
            ids[count++] = id;
            changed = 1;
        }
    }

    int success = 1;

    if (changed) {
        success = save_postings(path, ids, count);
    }

    free(ids);

    if (existing) {
        free(existing->book_ids);
        free(existing);
    }

    return success;
}

int hierarchical_update(
    const InvertedIndex *new_index,
    const char *directory
) {
    if (!new_index || !directory) {
        return 0;
    }

    if (!create_directory(directory)) {
        return 0;
    }

    for (size_t i = 0; i < new_index->count; i++) {
        const IndexEntry *entry = &new_index->entries[i];

        char folder[MAX_PATH_LENGTH];
        char path[MAX_PATH_LENGTH];

        if (!build_paths(
            directory,
            entry->term,
            folder,
            sizeof(folder),
            path,
            sizeof(path)
        )) {
            return 0;
        }

        if (!create_directory(folder)) {
            return 0;
        }

        if (!update_postings(path, &entry->postings)) {
            return 0;
        }
    }

    return 1;
}

int hierarchical_save(
    const InvertedIndex *index,
    const char *directory
) {
    if (!index || !directory) {
        return 0;
    }

    DWORD attributes = GetFileAttributesA(directory);

    if (attributes != INVALID_FILE_ATTRIBUTES) {
        char command[MAX_PATH_LENGTH + 64];

        int length = snprintf(
            command,
            sizeof(command),
            "cmd /C rmdir /S /Q \"%s\"",
            directory
        );

        if (length < 0 ||
            (size_t)length >= sizeof(command) ||
            system(command) != 0) {
            return 0;
        }
    }

    return hierarchical_update(index, directory);
}

PostingList *hierarchical_search(
    const char *directory,
    const char *term
) {
    if (!directory || !term || !term[0]) {
        return NULL;
    }

    size_t length = strlen(term);

    char *normalized = malloc(length + 1);

    if (!normalized) {
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        normalized[i] = (char)tolower(
            (unsigned char)term[i]
        );
    }

    normalized[length] = '\0';

    char folder[MAX_PATH_LENGTH];
    char path[MAX_PATH_LENGTH];

    int valid = build_paths(
        directory,
        normalized,
        folder,
        sizeof(folder),
        path,
        sizeof(path)
    );

    free(normalized);

    if (!valid) {
        return NULL;
    }

    return load_postings(path);
}
