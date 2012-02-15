#ifndef WORD_H
#define WORD_H

/* Word (token) and its metadata. */
typedef struct word {
    char *name;
    uint i;                 /* token ID or flags */
    short stop;             /* is it a stop word? */
    struct h_array *a;      /* array of occurrence hashes */
} word;

hash_t word_hash(char *w);

set *word_set_init(int sz);

word *word_new(char *w, size_t len, uint data);

void word_free(void *w);

word *word_add(set *wt, char *w, size_t len);

word *word_get(set *wt, char *wname);

int word_known(set *wt, char *wname);

void word_print_and_zero(set *wt);

char *default_gln_dir();

#endif
