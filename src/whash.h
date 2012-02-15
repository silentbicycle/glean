#ifndef WHASH_H
#define WHASH_H

typedef struct word {
    char *name;
    uint i;                 /* token ID or flags */
    short stop;             /* is it a stop word? */
    struct h_array *a;      /* array of occurrence hashes */
} word;

hash_t hash_word(char *w);
set *init_word_set(int sz);
word *new_word(char *w, size_t len, uint data);
void free_word(void *w);
word *add_word(set *wt, char *w, size_t len);
word *get_word(set *wt, char *wname);
int known_word(set *wt, char *wname);
void print_and_zero_words(set *wt);
char *default_gln_dir();

#endif
