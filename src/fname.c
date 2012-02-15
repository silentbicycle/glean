#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <err.h>
#include <string.h>

#include "glean.h"
#include "set.h"
#include "whash.h"
#include "fname.h"

static hash_t fname_hash(void *v) {
    return hash_word(((fname *)v)->name);
}

static int fname_cmp(void *a, void *b) {
    char *na = ((fname*)a)->name, *nb = ((fname*)b)->name;
    
    if (DEBUG) fprintf(stderr, "cmp: %s / %s -> %d\n",
        na, nb, strcmp(na, nb));
    
    return strcmp(na, nb);
}

set *fname_new_set(int sz_factor) {
    return set_new(sz_factor, fname_hash, fname_cmp);
}

fname *fname_new(char *n, size_t len) {
    fname *res = alloc(sizeof(*res), 'f');
    char *name = alloc(len + 1, 'n');
    strncpy(name, n, len + 1); /* strlcpy */
    res->name = name;
    return res;
}

void fname_free(void *f) {
    fname *fn = (fname *)f;
    free(fn->name);
    free(fn);
}

fname *fname_add(set *s, fname *f) {
    int res;
    if (DEBUG) fprintf(stderr,
        "Adding filename %s (%ld)\n", f->name, strlen(f->name));
    res = set_store(s, f);
    if (res == TABLE_SET_FAIL) err(1, "set_store failure");
    return f;
}