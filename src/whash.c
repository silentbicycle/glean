#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <err.h>
#include <string.h>

#include "glean.h"
#include "hash.h"
#include "whash.h"
#include "tokenize.h"
#include "array.h"

/* 113, 139, 173 all seem to work well - they're relatively prime to
 * bytes used in hashed data. */
#define HASH_MULTIPLIER 139

hash_t hash_word(char *w) {
    hash_t i, h = 0;
    for (i=0; w[i] != '\0'; i++)
        h = HASH_MULTIPLIER*h + (uint) w[i];
    if (DEBUG_HASH) printf("Hashed '%s' -> %u\n", w, h);
    if (HASH_BYTES == 2) h %= 0xffff;
    return h;
}

static uint word_hash(void *v) {
    return hash_word(((word *)v)->name);
}

static int word_cmp(void *a, void *b) {
    char *na = ((word*)a)->name, *nb = ((word*)b)->name;
    return strcmp(na, nb);
}

table *init_word_table(int sz_factor) {
    return table_init(sz_factor, word_hash, word_cmp);
}

word *new_word(char *w, size_t len, uint data) {
    word *ws = alloc(sizeof(word), 'w');
    char *nbuf = alloc(len + 1, 'n');
    assert(len > 0);
    strncpy(nbuf, w, len);
    nbuf[len] = '\0';
    ws->name = nbuf;
    ws->stop = 0;
    ws->a = h_array_init(2);
    assert(ws->a);
    ws->i = data;
    if (DEBUG) fprintf(stderr, "Created word %p %s %d\n",
        (void *) ws, ws->name, ws->i);
    return ws;
}

void free_word(void *v) {
    word *w = (word *)v;
    assert(w); assert(w->name);
    free(w->name);
    if (w->a) h_array_free(w->a);
    free(w);
}

/* Add an occurance of a word to the known words, allocating it if necessary. */
word *add_word(table *wt, char *w, size_t len) {
    char wbuf[MAX_WORD_SZ];
    word *nw;
    int res;
    
    assert(len > 0);
    strncpy(wbuf, w, len);
    wbuf[len] = '\0';
    nw = get_word(wt, wbuf);
    if (nw == NULL) {             /* nonexistent */
        nw = new_word(wbuf, len, 1);
        if (DEBUG)
            fprintf(stderr, "-- Adding word %s (%lu) -> %s\n", wbuf, len, nw->name);
        res = table_set(wt, nw);
        if (res == TABLE_SET_FAIL)
            err(1, "table_set failure");
    } else if (nw->i == 0) {     /* present but cleared */
        nw->i = 1;
    } else {
        nw->i++;             /* increment occurrance count */
    }
    return nw;
}

word *get_word(table *wt, char *wname) {
    word w, *res;
    w.name = wname;
    res = (word*)table_get(wt, &w);
    if (res != NULL) {
        if (DEBUG) fprintf(stderr, "Expected: %s\tGOT: %p, %p, %s\n",
            wname, (void *)res, (void *)res->name, res->name);
    }
    return res;
}

int known_word(table *wt, char *wname) {
    word *w = get_word(wt, wname);
    return w != NULL && w->i > 0;
}

static void print_and_zero(void *v) {
    word *w = (word *)v;
    if (w->i) printf("%s %d\n", w->name, w->i);
    w->i = 0;
}

/* Print known words & location flags, clearing the flags along the way. */
void print_and_zero_words(table *wt) { table_apply(wt, print_and_zero); }

char *default_gln_dir() { /* ~ */
    char *hm = getenv("HOME");
    if (hm == NULL) err(1, "no home?");
    return hm;
}
