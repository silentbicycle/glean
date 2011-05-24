#ifndef WHASH_H
#define WHASH_H

typedef struct word {
        char *name;
        uint i;                 /* token ID or flags */
        short stop;             /* is it a stop word? */
        struct h_array *a;      /* array of occurrence hashes */
} word;

hash_t hash_word(char *w);
table *init_word_table(int sz);
word *new_word(char *w, size_t len, uint data);
void free_word(void *w);
word *add_word(table *wt, char *w, size_t len);
word *get_word(table *wt, char *wname);
int known_word(table *wt, char *wname);
void print_and_zero_words(table *wt);
char *default_gln_dir();

#endif
