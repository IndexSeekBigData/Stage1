
#ifndef HIERARCHICAL_H
#define HIERARCHICAL_H

#include "inverted_index.h"

int hierarchical_save(
    const InvertedIndex *index,
    const char *directory
);

int hierarchical_update(
    const InvertedIndex *new_index,
    const char *directory
);

PostingList *hierarchical_search(
    const char *directory,
    const char *term
);

#endif