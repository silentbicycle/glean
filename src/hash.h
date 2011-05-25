#ifndef HASH_H
#define HASH_H

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

typedef struct table {
        int sz;
        int ms;              /* max size */
        short mcl;           /* max chain length */
        uint (*hash)(void *);
        int (*cmp)(void *, void *);
        tlink **b;            /* buckets */
} table;

table *table_init(int sz_factor, uint (*hash)(void *), int (*cmp)(void *, void *));
void table_set_max_size(table *t, int ms);
void table_set_max_chain_length(table *t, int cl);
void *table_get(table *t, void *v);
int table_set(table *t, void *v);
int table_known(table *t, void *v);
void table_apply(table *t, void (*f)(void *val));
void table_free(table *t, void (*free_val)(void *val));
void table_stats(table *t, int verbose);

#endif
