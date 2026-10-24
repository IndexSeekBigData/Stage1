
#include "monolithic_json.h"
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

static int build_path(
    const char *directory,
    char *path,
    size_t capacity
) {
    int length = snprintf(
        path,
        capacity,
        "%s\\inverted_index.json",
        directory
    );

    return length >= 0 && (size_t)length < capacity;
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

    size_t bytes_read = fread(content, 1, (size_t)size, file);

    fclose(file);

    if (bytes_read != (size_t)size) {
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

static int write_json(
    const cJSON *json,
    const char *path
) {
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

    int success =
        fwrite(content, 1, length, file) == length;

    if (fclose(file) != 0) {
        success = 0;
    }

    cJSON_free(content);

    return success;
}

static cJSON *create_postings_array(
    const PostingList *postings
) {
    cJSON *array = cJSON_CreateArray();

    if (!array) {
        return NULL;
    }

    int *ids = malloc(postings->count * sizeof(int));

    if (postings->count > 0 && !ids) {
        cJSON_Delete(array);
        return NULL;
    }

    memcpy(
        ids,
        postings->book_ids,
        postings->count * sizeof(int)
    );

    qsort(
        ids,
        postings->count,
        sizeof(int),
        compare_ids
    );

    for (size_t i = 0; i < postings->count; i++) {
        char id[32];

        snprintf(id, sizeof(id), "%d", ids[i]);

        cJSON *item = cJSON_CreateString(id);

        if (!item) {
            free(ids);
            cJSON_Delete(array);
            return NULL;
        }

        cJSON_AddItemToArray(array, item);
    }

    free(ids);

    return array;
}

static int add_postings_to_json(
    cJSON *json,
    const InvertedIndex *index
) {
    for (size_t i = 0; i < index->count; i++) {
        const IndexEntry *entry = &index->entries[i];

        cJSON *existing = cJSON_GetObjectItemCaseSensitive(
            json,
            entry->term
        );

        size_t existing_count = 0;

        if (cJSON_IsArray(existing)) {
            existing_count = (size_t)cJSON_GetArraySize(existing);
        }

        size_t total_capacity =
            existing_count + entry->postings.count;

        int *ids = malloc(total_capacity * sizeof(int));

        if (total_capacity > 0 && !ids) {
            return 0;
        }

        size_t count = 0;

        if (cJSON_IsArray(existing)) {
            cJSON *item = NULL;

            cJSON_ArrayForEach(item, existing) {
                if (!cJSON_IsString(item) ||
                    !item->valuestring) {
                    free(ids);
                    return 0;
                }

                char *end;
                long value = strtol(
                    item->valuestring,
                    &end,
                    10
                );

                if (*end != '\0' ||
                    value < 0 ||
                    value > INT_MAX) {
                    free(ids);
                    return 0;
                }

                ids[count++] = (int)value;
            }
        }

        for (size_t j = 0; j < entry->postings.count; j++) {
            ids[count++] = entry->postings.book_ids[j];
        }

        qsort(ids, count, sizeof(int), compare_ids);

        cJSON *updated = cJSON_CreateArray();

        if (!updated) {
            free(ids);
            return 0;
        }

        for (size_t j = 0; j < count; j++) {
            if (j > 0 && ids[j] == ids[j - 1]) {
                continue;
            }

            char id[32];

            snprintf(id, sizeof(id), "%d", ids[j]);

            cJSON *item = cJSON_CreateString(id);

            if (!item) {
                free(ids);
                cJSON_Delete(updated);
                return 0;
            }

            cJSON_AddItemToArray(updated, item);
        }

        free(ids);

        if (existing) {
            if (!cJSON_ReplaceItemInObjectCaseSensitive(
                json,
                entry->term,
                updated
            )) {
                cJSON_Delete(updated);
                return 0;
            }
        } else {
            if (!cJSON_AddItemToObject(
                json,
                entry->term,
                updated
            )) {
                cJSON_Delete(updated);
                return 0;
            }
        }
    }

    return 1;
}

int monolithic_save(
    const InvertedIndex *index,
    const char *directory
) {
    if (!index || !directory) {
        return 0;
    }

    if (!CreateDirectoryA(directory, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return 0;
    }

    char path[MAX_PATH_LENGTH];

    if (!build_path(directory, path, sizeof(path))) {
        return 0;
    }

    cJSON *json = cJSON_CreateObject();

    if (!json) {
        return 0;
    }

    int success = 1;

    for (size_t i = 0; i < index->count; i++) {
        const IndexEntry *entry = &index->entries[i];

        cJSON *array = create_postings_array(
            &entry->postings
        );

        if (!array) {
            success = 0;
            break;
        }

        if (!cJSON_AddItemToObject(
            json,
            entry->term,
            array
        )) {
            cJSON_Delete(array);
            success = 0;
            break;
        }
    }

    if (success) {
        success = write_json(json, path);
    }

    cJSON_Delete(json);

    return success;
}

int monolithic_update(
    const InvertedIndex *new_index,
    const char *directory
) {
    if (!new_index || !directory) {
        return 0;
    }

    if (!CreateDirectoryA(directory, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return 0;
    }

    char path[MAX_PATH_LENGTH];

    if (!build_path(directory, path, sizeof(path))) {
        return 0;
    }

    cJSON *json = load_json(path);

    if (!json) {
        DWORD attributes = GetFileAttributesA(path);

        if (attributes != INVALID_FILE_ATTRIBUTES) {
            return 0;
        }

        json = cJSON_CreateObject();
    }

    if (!json) {
        return 0;
    }

    int success = add_postings_to_json(
        json,
        new_index
    );

    if (success) {
        success = write_json(json, path);
    }

    cJSON_Delete(json);

    return success;
}

PostingList *monolithic_search(
    const char *directory,
    const char *term
) {
    if (!directory || !term) {
        return NULL;
    }

    char path[MAX_PATH_LENGTH];

    if (!build_path(directory, path, sizeof(path))) {
        return NULL;
    }

    cJSON *json = load_json(path);

    if (!json) {
        return NULL;
    }

    size_t length = strlen(term);

    char *normalized = malloc(length + 1);

    if (!normalized) {
        cJSON_Delete(json);
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        normalized[i] = (char)tolower(
            (unsigned char)term[i]
        );
    }

    normalized[length] = '\0';

    cJSON *array = cJSON_GetObjectItemCaseSensitive(
        json,
        normalized
    );

    free(normalized);

    if (!cJSON_IsArray(array)) {
        cJSON_Delete(json);
        return NULL;
    }

    PostingList *result = calloc(1, sizeof(PostingList));

    if (!result) {
        cJSON_Delete(json);
        return NULL;
    }

    size_t count = (size_t)cJSON_GetArraySize(array);

    if (count > 0) {
        result->book_ids = malloc(count * sizeof(int));

        if (!result->book_ids) {
            free(result);
            cJSON_Delete(json);
            return NULL;
        }
    }

    result->capacity = count;

    cJSON *item = NULL;

    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) ||
            !item->valuestring) {
            free(result->book_ids);
            free(result);
            cJSON_Delete(json);
            return NULL;
        }

        char *end;
        long value = strtol(
            item->valuestring,
            &end,
            10
        );

        if (*end != '\0' ||
            value < 0 ||
            value > INT_MAX) {
            free(result->book_ids);
            free(result);
            cJSON_Delete(json);
            return NULL;
        }

        result->book_ids[result->count++] = (int)value;
    }

    cJSON_Delete(json);

    return result;
}
