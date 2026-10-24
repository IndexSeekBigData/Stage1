
#ifndef MONOLITHIC_JSON_H
#define MONOLITHIC_JSON_H

#include "inverted_index.h"

int monolithic_save(
    const InvertedIndex *index,
    const char *directory
);

int monolithic_update(
    const InvertedIndex *new_index,
    const char *directory
);

PostingList *monolithic_search(
    const char *directory,
    const char *term
);

#endif
