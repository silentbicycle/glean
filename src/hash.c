#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <err.h>
#include <string.h>
#include <sys/types.h>

#include "glean.h"
#include "hash.h"

/* Chaining, resizable hash table for a set of unique values.
 * Adding a duplicated value is an unchecked error. */

/* table_remove NYI, not used. Append only. */

/* Largest primes preceeding increasing powers of 2.
 * Duplicate value -> at max. Only add primes > 2^16
 * when hashes are larger than 2 bytes, having more
 * buckets than possible hashes is counterproductive. */
static uint primes[] = { 3, 7, 13, 31, 61, 127, 251, 509, 1021, 2039,
                         4093, 8191, 16381, 32749, 65521,
                         131071, 262139, 524287, 1048573, 2097143, 4194301,
                         8388593, 16777213, 33554393, 67108859, 134217689,
                         268435399, 536870909, 1073741789, 1073741789,
                         0 };

#ifndef ALLOC
#define ALLOC(x) basic_alloc(x)
#endif

#ifndef FREE
#define FREE(x) free(x)
#endif

static void *basic_alloc(size_t sz) {
    void *p = malloc(sz);
    if (p == NULL) err(1, "alloc fail");
    return p;
}

table *table_init(int sz_factor, table_hash *hash, table_cmp *cmp) {
    int i, sz;
    table *t = NULL;
    tlink **b;
    assert(sz_factor >= 0 && sz_factor <= (sizeof(primes) / sizeof(int)));
    sz = primes[sz_factor];
    b = ALLOC(sz*sizeof(void *));
    t = ALLOC(sizeof(table));
    assert(hash); assert(cmp);
    t->sz=sz; t->hash = hash; t->cmp = cmp; t->b = b;
    t->ms = primes[(sizeof(primes)/sizeof(primes[0]))-2];
    t->mcl = DEF_GROW_LEN;
    for (i=0; i<sz; i++) t->b[i] = NULL;
    return t;
}

/* Get a value associated w/ a key, moving its tlink to the
 * front of its chain. Return NULL if not found. */
void *table_get(table *t, void *v) {
    uint h, b;
    tlink *cur = NULL, *prev = NULL, *head;
    assert(v); assert(t);
    h = t->hash(v);
    b = h % t->sz;          /* bucket id */
    head = t->b[b];
    /* fprintf(stderr, "Hashed to bucket %u\n", b); */
    for (cur = head; cur != NULL; cur = cur->next) {
        assert(v); assert(cur->v);
        if (t->cmp(v, cur->v) == 0) break;
        prev = cur;
    }
    if (cur != NULL) {
        if (prev != NULL) {    /* move to front */
            prev->next = cur->next;
            t->b[b] = cur;
            cur->next = head;
        }
        return cur->v;
    }
    return NULL;
}

void table_set_max_size(table *t, int ms) { t->ms = ms; }
void table_set_max_chain_length(table *t, int cl) { t->mcl = cl; }
int table_known(table *t, void *v) { return table_get(t, v) != NULL; }

/* Return next larger size, or same size if maxed out. */
static int next_sz(int sz) {
    int i;
    int nsz = sz;
    for (i=0; primes[i] != 0; i++) {
        if (primes[i] > sz) { nsz = primes[i]; break; }
    }
    return nsz;
}

/* While resizing, move a value to its new bucket. */
static void table_move(table *t, tlink *n) {
    uint h, b;
    tlink *cur;
    assert(n); assert(t);
    h = t->hash(n->v);
    b = h % t->sz;
    cur = t->b[b];
    t->b[b] = n;
    n->next = cur;          /* front */
}

/* Switch to a larger bucket array and rehash contents. */
static int table_resize(table *t, int sz) {
    int i, old_sz;
    tlink **nb;
    tlink *cur, **oldb = (tlink**)t->b, *next;
    if (DEBUG) fprintf(stderr, "\n\n-- Resizing from %d to %d\n\n", t->sz, sz);
    old_sz = t->sz;
    if (sz == old_sz) { return TABLE_FULL; }
    nb = ALLOC(sz*sizeof(void *));
    for (i=0; i<sz; i++) nb[i] = NULL;
    
    t->b = nb;
    t->sz = sz;
    for (i=0; i<old_sz; i++) {
        cur = oldb[i];
        while (cur != NULL) {
            next = cur->next;
            cur->next = NULL;
            table_move(t, cur);
            cur = next;
        }
    }
    FREE(oldb);
    return TABLE_RESIZED;
}

static int table_grow(table *t) {
    int gsz = next_sz(t->sz);
    if (gsz > t->ms || gsz == t->sz) return TABLE_FULL;
    return table_resize(t, gsz);
}

int table_set(table *t, void *v) {
    uint h, b, len=0;
    tlink *n, *cur, *tail = NULL;
    int status = 0;
    
    assert(v); assert(t);
    h = t->hash(v);
    b = h % t->sz;
    n = ALLOC(sizeof(tlink));
    n->next = NULL;
    n->v = v;
    
    /* If the table is already at the max size, just put it at the
     * head, don't bother walking the whole chain. */
    if (t->sz == t->ms) {
        cur = t->b[b];
        t->b[b] = n;
        n->next = cur;
        return TABLE_SET | TABLE_FULL;
    }
    
    /* Otherwise, note the length, to see if it's time to resize. */
    for (cur = t->b[b]; cur != NULL; cur=cur->next) {
        len++;
        tail = cur;
    }
    if (tail) {
        tail->next = n;
    } else {
        t->b[b] = n;
    }
    status |= TABLE_SET;
    if (len > t->mcl) {
        status |= table_grow(t);
        if (DEBUG) table_stats(t, 0);
    }
    return status;
}

/* Apply cb(*v) to every element in each bucket chain. */
void table_apply(table *t, table_apply_cb *cb) {
    int i;
    tlink *cur;
    assert(t); assert(cb);
    for (i=0; i<t->sz; i++) {
        for (cur = t->b[i]; cur != NULL; cur=cur->next) {
            cb(cur->v);
        }
    }
}

void table_free(table *t, table_free_cb *cb) {
    int i;
    tlink *cur, *n;
    assert(t);
    for (i=0; i<t->sz; i++) {
        cur = t->b[i];
        while (cur) {
            if (cb) cb(cur->v);
            n = cur->next;
            FREE(cur);
            cur = n;
        }
    }
    FREE(t->b);
    FREE(t);
}

/* Dump stats about how evenly the table is filled. */
void table_stats(table *t, int verbose) {
    long i, len=0, tot=0, minlen=t->mcl, maxlen=0;
    tlink *cur;
    fprintf(stderr, "table: %d buckets, max chain len %d, max size %d\n",
        t->sz, t->mcl, t->ms);
    for (i=0; i<t->sz; i++) {
        len = 0;
        for (cur = t->b[i]; cur != NULL; cur=cur->next) len++;
        if (verbose) fprintf(stderr, "bucket %ld: %ld\n", i, len);
        tot += len;
        if (len < minlen) minlen = len;
        else if (len > maxlen) maxlen = len;
    }
    fprintf(stderr, "----\ttotal: %ld\tavg: %.2f\tmin: %ld\tmax: %ld\n",
        tot, tot / (float)t->sz, minlen, maxlen);
}
