#include "datalake.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int create_directory(const char *path) {
    if (CreateDirectoryA(path, NULL)) {
        return 1;
    }

    return GetLastError() == ERROR_ALREADY_EXISTS;
}

int copy_file(const char *source, const char *destination) {
    return CopyFileA(source, destination, FALSE) != 0;
}

int file_exists(const char *path) {
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

long long file_size(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA data;

    if (!GetFileAttributesExA(
            path,
            GetFileExInfoStandard,
            &data
        )) {
        return -1;
    }

    ULARGE_INTEGER size;

    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;

    return (long long) size.QuadPart;
}

int delete_directory(const char *path) {
    char command[4096];

    if (snprintf(
            command,
            sizeof(command),
            "cmd /C rmdir /S /Q \"%s\"",
            path
        ) >= (int)sizeof(command)) {
        return 0;
    }

    return system(command) == 0;
}
