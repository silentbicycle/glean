#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <err.h>
#include <string.h>

#include "glean.h"
#include "set.h"
#include "word.h"
#include "tokenize.h"
#include "array.h"

/* 113, 139, 173 all seem to work well - they're relatively prime to
 * bytes used in hashed data. */
#define HASH_MULTIPLIER 139

hash_t word_hash(char *w) {
    hash_t i, h = 0;
    for (i=0; w[i] != '\0'; i++)
        h = HASH_MULTIPLIER*h + (uint) w[i];
    if (DEBUG_HASH) printf("Hashed '%s' -> %u\n", w, h);
    if (HASH_BYTES == 2) h %= 0xffff;
    return h;
}

static hash_t hash_cb(void *v) {
    return word_hash(((word *)v)->name);
}

static int cmp_cb(void *a, void *b) {
    char *na = ((word*)a)->name, *nb = ((word*)b)->name;
    return strcmp(na, nb);
}

/* Create a set<word>, expecting to store at least 2^sz_factor values.
 * Returns NULL on error. */
set *word_set_init(int sz_factor) {
    return set_new(sz_factor, hash_cb, cmp_cb);
}

/* Create a new word from the LEN-byte string at W,
 * with starting count COUNT. */
word *word_new(char *w, size_t len, uint count) {
    word *ws = alloc(sizeof(word), 'w');
    char *nbuf = alloc(len + 1, 'n');
    assert(len > 0);
    strncpy(nbuf, w, len);
    nbuf[len] = '\0';
    ws->name = nbuf;
    ws->stop = 0;
    ws->a = h_array_new(2);
    assert(ws->a);
    ws->count = count;
    if (DEBUG) fprintf(stderr, "Created word %p %s %u\n",
        (void *) ws, ws->name, ws->count);
    return ws;
}

/* Free a word. */
void word_free(void *v) {
    word *w = (word *)v;
    assert(w); assert(w->name);
    free(w->name);
    if (w->a) h_array_free(w->a);
    free(w);
}

/* Add an occurance of a word to the known words, allocating it if necessary. */
word *word_add(set *s, char *w, size_t len) {
    char wbuf[MAX_WORD_SZ];
    word *nw;
    int res;
    
    assert(len > 0);
    strncpy(wbuf, w, len);
    wbuf[len] = '\0';
    nw = word_get(s, wbuf);
    if (nw == NULL) {             /* nonexistent */
        nw = word_new(wbuf, len, 1);
        if (DEBUG)
            fprintf(stderr, "-- Adding word %s (%lu) -> %s\n", wbuf, len, nw->name);
        res = set_store(s, nw);
        if (res == TABLE_SET_FAIL)
            err(1, "set_store failure");
    } else if (nw->count == 0) {    /* present but cleared */
        nw->count = 1;
    } else {
        nw->count++;                /* increment occurrance count */
    }
    return nw;
}

/* Get the interned data for a word. */
word *word_get(set *s, char *wname) {
    word w, *res;
    w.name = wname;
    res = (word*)set_get(s, &w);
    if (res != NULL) {
        if (DEBUG) fprintf(stderr, "Expected: %s\tGOT: %p, %p, %s\n",
            wname, (void *)res, (void *)res->name, res->name);
    }
    return res;
}

/* Is a word already in the set? */
int word_known(set *s, char *wname) {
    word *w = word_get(s, wname);
    return w != NULL && w->count > 0;
}

/* Print each (word, count) pair and zero their counts. */
static void print_and_zero(void *v, void *unused) {
    word *w = (word *)v;
    if (w->count > 0) printf("%s %d\n", w->name, w->count);
    w->count = 0;
}

/* Print known words & location flags, clearing the flags along the way. */
void word_print_and_zero(set *s) { set_apply(s, print_and_zero, NULL); }
