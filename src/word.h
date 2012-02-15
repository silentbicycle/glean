#ifndef WORD_H
#define WORD_H

/* Word (token) and its metadata. */
typedef struct word {
    char *name;                 /* internal copy of word string */
    uint count;                 /* word occurrence count */
    short stop;                 /* is it a stop word? */
    struct h_array *a;          /* array of occurrence hashes */
} word;

/* Hash a zero-terminated string. */
hash_t word_hash(char *w);

/* Create a set<word>, expecting to store at least 2^sz_factor values.
 * Returns NULL on error. */
set *word_set_init(int sz_factor);

/* Create a new word from the LEN-byte string at W,
 * with starting count COUNT. */
word *word_new(char *w, size_t len, uint count);

/* Free a word. */
void word_free(void *w);

/* Add an occurance to the set<word>, allocating if necessary. */
word *word_add(set *s, char *w, size_t len);

/* Get the interned data for a word. */
word *word_get(set *s, char *wname);

/* Is a word already in the set? */
int word_known(set *s, char *wname);

/* Print each (word, count) pair and zero their counts. */
void word_print_and_zero(set *s);

#endif
