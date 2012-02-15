#ifndef FHASH_H
#define FHASH_H

typedef struct fname {
    char *name;
} fname;

uint fname_hash(void *v);
set *init_fname_set(int sz_factor);
fname *new_fname(char *n, size_t len);
void free_fname(void *w);
fname *add_fname(set *wt, fname *f);

#endif
