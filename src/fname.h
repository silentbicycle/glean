#ifndef FNAME_H
#define FNAME_H

/* Box the filename pointer in a struct, for added typechecking. */
typedef struct fname {
    char *name;
} fname;

/* Make a new filename set. */
set *fname_new_set(int sz_factor);

/* Make an internal copy of filename N. */
fname *fname_new(char *n, size_t len);

/* Add filename F to the set. */
fname *fname_add(set *s, fname *f);

/* Callback for freeing filenames in an fname* set. */
void fname_free_cb(void *f);

#endif
