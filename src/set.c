#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <err.h>
#include <string.h>

#include "glean.h"
#include "set.h"

/* Externally-chaining, resizable hash table for a set of unique keys.
 * I'm not using a more general-purpose hash table because only storing
 * the keys (no associated value) leads to a substantial memory savings.
 *
 * Chaining is used because once the whole data set is loaded, each
 * bucket is flattened and individually compressed.
 * 
 * Adding a duplicated key is an unchecked error. */

/* Largest primes preceeding increasing powers of 2.
 * Duplicate value -> at max. */
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

set *set_init(int sz_factor, set_hash *hash, set_cmp *cmp) {
    int i, sz;
    set *s = NULL;
    s_link **b;
    assert(sz_factor >= 0 && sz_factor <= (sizeof(primes) / sizeof(int)));
    sz = primes[sz_factor];
    b = ALLOC(sz*sizeof(void *));
    s = ALLOC(sizeof(set));
    assert(hash); assert(cmp);
    s->sz=sz; s->hash = hash; s->cmp = cmp; s->b = b;
    s->ms = primes[(sizeof(primes)/sizeof(primes[0]))-2];
    s->mcl = DEF_GROW_LEN;
    for (i=0; i<sz; i++) s->b[i] = NULL;
    return s;
}

/* Get a value associated w/ a key, moving its s_link to the
 * front of its chain. Return NULL if not found. */
void *set_get(set *s, void *v) {
    uint h, b;
    s_link *cur = NULL, *prev = NULL, *head;
    assert(v); assert(s);
    h = s->hash(v);
    b = h % s->sz;          /* bucket id */
    head = s->b[b];
    /* fprintf(stderr, "Hashed to bucket %u\n", b); */
    for (cur = head; cur != NULL; cur = cur->next) {
        assert(v); assert(cur->v);
        if (s->cmp(v, cur->v) == 0) break;
        prev = cur;
    }
    if (cur != NULL) {
        if (prev != NULL) {    /* move to front */
            prev->next = cur->next;
            s->b[b] = cur;
            cur->next = head;
        }
        return cur->v;
    }
    return NULL;
}

int set_known(set *s, void *v) { return set_get(s, v) != NULL; }

/* Return next larger size, or same size if maxed out. */
static ulong next_sz(ulong sz) {
    int i;
    ulong nsz = sz;
    for (i=0; primes[i] != 0; i++) {
        if (primes[i] > sz) { nsz = primes[i]; break; }
    }
    return nsz;
}

/* While resizing, move a value to its new bucket. */
static void set_move(set *s, s_link *n) {
    uint h, b;
    s_link *cur;
    assert(n); assert(s);
    h = s->hash(n->v);
    b = h % s->sz;
    cur = s->b[b];
    s->b[b] = n;
    n->next = cur;          /* front */
}

/* Switch to a larger bucket array and rehash contents. */
static int set_resize(set *s, ulong sz) {
    int i, old_sz;
    s_link **nb;
    s_link *cur, **oldb = (s_link**)s->b, *next;
    if (DEBUG) fprintf(stderr, "\n\n-- Resizing from %lu to %lu\n\n", s->sz, sz);
    old_sz = s->sz;
    if (sz == old_sz) { return TABLE_FULL; }
    nb = ALLOC(sz*sizeof(void *));
    for (i=0; i<sz; i++) nb[i] = NULL;
    
    s->b = nb;
    s->sz = sz;
    for (i=0; i<old_sz; i++) {
        cur = oldb[i];
        while (cur != NULL) {
            next = cur->next;
            cur->next = NULL;
            set_move(s, cur);
            cur = next;
        }
    }
    FREE(oldb);
    return TABLE_RESIZED;
}

static int set_grow(set *s) {
    ulong gsz = next_sz(s->sz);
    if (gsz > s->ms || gsz == s->sz) return TABLE_FULL;
    return set_resize(s, gsz);
}

int set_store(set *s, void *v) {
    uint h, b, len=0;
    s_link *n, *cur, *tail = NULL;
    int status = 0;
    
    assert(v); assert(s);
    h = s->hash(v);
    b = h % s->sz;
    n = ALLOC(sizeof(s_link));
    n->next = NULL;
    n->v = v;
    
    /* If the hash table is already at the max size, just put it at the
     * head, don't bother walking the whole chain. */
    if (s->sz == s->ms) {
        cur = s->b[b];
        s->b[b] = n;
        n->next = cur;
        return TABLE_SET | TABLE_FULL;
    }
    
    /* Otherwise, note the length, to see if it's time to resize. */
    for (cur = s->b[b]; cur != NULL; cur=cur->next) {
        len++;
        tail = cur;
    }
    if (tail) {
        tail->next = n;
    } else {
        s->b[b] = n;
    }
    status |= TABLE_SET;
    if (len > s->mcl) {
        status |= set_grow(s);
        if (DEBUG) set_stats(s, 0);
    }
    return status;
}

/* Apply cb(*v) to every element in each bucket chain. */
void set_apply(set *s, set_apply_cb *cb) {
    int i;
    s_link *cur;
    assert(s); assert(cb);
    for (i=0; i<s->sz; i++) {
        for (cur = s->b[i]; cur != NULL; cur=cur->next) {
            cb(cur->v);
        }
    }
}

void set_free(set *s, set_free_cb *cb) {
    int i;
    s_link *cur, *n;
    assert(s);
    for (i=0; i<s->sz; i++) {
        cur = s->b[i];
        while (cur) {
            if (cb) cb(cur->v);
            n = cur->next;
            FREE(cur);
            cur = n;
        }
    }
    FREE(s->b);
    FREE(s);
}

/* Dump stats about how evenly the table is filled. */
void set_stats(set *s, int verbose) {
    long i, len=0, tot=0, minlen=s->mcl, maxlen=0;
    s_link *cur;
    fprintf(stderr, "set: %lu buckets, max chain len %d, max size %lu\n",
        s->sz, s->mcl, s->ms);
    for (i=0; i<s->sz; i++) {
        len = 0;
        for (cur = s->b[i]; cur != NULL; cur=cur->next) len++;
        if (verbose) fprintf(stderr, "bucket %ld: %ld\n", i, len);
        tot += len;
        if (len < minlen) minlen = len;
        else if (len > maxlen) maxlen = len;
    }
    fprintf(stderr, "----\ttotal: %ld\tavg: %.2f\tmin: %ld\tmax: %ld\n",
        tot, tot / (float)s->sz, minlen, maxlen);
}
