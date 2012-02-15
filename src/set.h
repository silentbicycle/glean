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
typedef struct tlink {
    void *v;
    struct tlink *next;
} tlink;

/* Hash function - should take a void * and return an unsigned int hash. */
typedef hash_t (table_hash)(void *v);
                         
/* Value comparison function - should return <0 if a is less
 * than b, >0 if a is > b, and 0 if a == b. */
typedef int (table_cmp)(void *a, void *b);

/* Apply callback - apply the function to the value v. */
typedef void table_apply_cb(void *v);

/* Table value free callback - free the value v. */
typedef void (table_free_cb)(void *v);

/* Hash-table set. */
typedef struct table {
    int sz;              /* current size (bucket count) */
    int ms;              /* max size (bucket count) */
    short mcl;           /* max chain length */
    table_hash *hash;    /* hash function */
    table_cmp *cmp;      /* comparison function */
    tlink **b;           /* buckets */
} table;

/* Initialize a hash table set, expecting to store at
 * least 2^sz_factor values. Returns NULL on error. */
table *table_init(int sz_factor, table_hash *hash, table_cmp *cmp);

/* Set the max length allowed for an individual bucket chain before
 * the hash table should grow and rehash. */
void table_set_max_chain_length(table *t, int cl);

/* Set the max bucket size for the table. */
void table_set_max_size(table *t, int ms);

/* Get the canonical version of the key, or NULL if unknown. */
void *table_get(table *t, void *v);

/* Store the key, return an int with bits set according to TABLE_* flags.
 * Returns TABLE_SET_FAIL (0) on error. */
int table_set(table *t, void *v);

/* Is a given key known? */
int table_known(table *t, void *v);

/* Apply the callback to every key. */
void table_apply(table *t, table_apply_cb *cb);

/* Free the table, calling CB (if set) on every key. */
void table_free(table *t, table_free_cb *cb);

/* Print debugging info about the hash table. */
void table_stats(table *t, int verbose);

#endif
