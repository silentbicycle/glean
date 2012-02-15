#ifndef SET_H
#define SET_H

/* Grow when any chain gets this long; the average chain
 * will be much smaller. A longer chain is slower, but will
 * lead to a database that compresses slightly better. */
#define DEF_GROW_LEN 50

/* Flags */
#define TABLE_SET_FAIL  0
#define TABLE_SET	0x01
#define TABLE_RESIZED	0x02
#define TABLE_FULL      0x04

/* Linked list, branching off table. */
typedef struct s_link {
    void *key;
    struct s_link *next;
} s_link;

/* Hash function - should take a void * and return an unsigned int hash. */
typedef hash_t (set_hash)(void *key);
                         
/* Value comparison function - should return <0 if a is less
 * than b, >0 if a is > b, and 0 if a == b. */
typedef int (set_cmp)(void *a, void *b);

/* Apply callback - apply the function to the value v. */
typedef void set_apply_cb(void *key);

/* Table value free callback - free the value v. */
typedef void (set_free_cb)(void *key);

/* Hash-table set.
 * 
 * The internals are exported because db.c's write_set_data currently
 * stores the databases by compressing every bucket as a whole, and
 * thus depends on the internal structure. */
typedef struct set {
    ulong sz;                   /* current size (bucket count) */
    ulong ms;                   /* max size (bucket count) */
    short mcl;                  /* max chain length */
    set_hash *hash;             /* hash function */
    set_cmp *cmp;               /* comparison function */
    s_link **b;                 /* buckets */
} set;

/* Initialize a hash table set, expecting to store at
 * least 2^sz_factor values. Returns NULL on error. */
set *set_init(int sz_factor, set_hash *hash, set_cmp *cmp);

/* Get the canonical version of the key, or NULL if unknown. */
void *set_get(set *t, void *key);

/* Store the key, return an int with bits set according to TABLE_* flags.
 * Returns TABLE_SET_FAIL (0) on error. */
int set_store(set *t, void *key);

/* Is a given key known? */
int set_known(set *t, void *key);

/* Apply the callback to every key. */
void set_apply(set *t, set_apply_cb *cb);

/* Free the set, calling CB on every key (if non-NULL). */
void set_free(set *t, set_free_cb *cb);

/* Print tuning statistics about the set's internals. */
void set_stats(set *t, int verbose);

#endif
