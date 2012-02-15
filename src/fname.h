#ifndef FHASH_H
#define FHASH_H

/* Box the filename pointer in a struct, for added typechecking. */
typedef struct fname {
    char *name;
} fname;

set *fname_new_set(int sz_factor);
fname *fname_new(char *n, size_t len);
void fname_free(void *w);
fname *fname_add(set *wt, fname *f);

#endif
