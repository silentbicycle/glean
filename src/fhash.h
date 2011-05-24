#ifndef FHASH_H
#define FHASH_H

typedef struct fname {
        char *name;
} fname;

uint fname_hash(void *v);
table *init_fname_table(int sz_factor);
fname *new_fname(char *n, size_t len);
void free_fname(void *w);
fname *add_fname(table *wt, fname *f);

#endif
