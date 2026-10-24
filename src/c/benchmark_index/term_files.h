
#ifndef TERM_FILES_H
#define TERM_FILES_H

#include "inverted_index.h"

int term_files_save(
    const InvertedIndex *index,
    const char *directory
);

int term_files_update(
    const InvertedIndex *new_index,
    const char *directory
);

PostingList *term_files_search(
    const char *directory,
    const char *term
);

#endif
