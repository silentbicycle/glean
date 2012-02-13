#ifndef TABLE_H
#define TABLE_H

/* Grow when any chain gets this long; average will be much smaller. */
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
typedef uint (table_hash)(void *v);
                         
/* Value comparison function - should return <0 if a is less
 * than b, >0 if a is > b, and 0 if a == b. */
typedef int (table_cmp)(void *a, void *b);

/* Apply callback - apply the function to the value v. */
typedef void table_apply_cb(void *v);

/* Table value free callback - free the value v. */
typedef void (table_free_cb)(void *v);

typedef struct table {
    int sz;
    int ms;              /* max size */
    short mcl;           /* max chain length */
    table_hash *hash;    /* hash function */
    table_cmp *cmp;      /* comparison function */
    tlink **b;           /* buckets */
} table;

table *table_init(int sz_factor, table_hash *hash, table_cmp *cmp);
void table_set_max_size(table *t, int ms);
void table_set_max_chain_length(table *t, int cl);
void *table_get(table *t, void *v);
int table_set(table *t, void *v);
int table_known(table *t, void *v);
void table_apply(table *t, table_apply_cb *cb);
void table_free(table *t, table_free_cb *cb);
void table_stats(table *t, int verbose);

#endif
