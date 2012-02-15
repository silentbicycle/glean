#ifndef ARRAY_H
#define ARRAY_H

/* Resizable uint -> hash_t array.
 * Separate from v_array since sizeof(hash_t) is much less than
 * sizeof(void *), and most live data is hash_t arrays - this leads
 * to major memory savings. */
typedef struct h_array {
    uint sz;                /* allocated size */
    uint len;               /* filled length */
    hash_t *hs;             /* file hashes */
} h_array;

h_array *h_array_new(uint sz);
void h_array_append(h_array *a, hash_t v);
hash_t h_array_get(h_array *a, uint i);
uint h_array_length(h_array *a);

/* Sort the hash array in place. */
void h_array_sort(h_array *a);

/* Remove duplicates from an array (which must be sorted). */
void h_array_uniq(h_array *a);

/* Get the union/intersection/complement of two sorted arrays. */
h_array *h_array_union(h_array *a, h_array *b);
h_array *h_array_intersection(h_array *a, h_array *b);
h_array *h_array_complement(h_array *a, h_array *b);

void h_array_free(h_array *a);

/* Dynamically resized void pointer array (vector).
 * When appending, the allocated memory for VS will be
 * doubled if LEN == SZ. */
typedef struct v_array {
    uint sz;                /* allocated size */
    uint len;               /* filled length */
    void **vs;              /* values */
} v_array;

/* Value comparison function - should return <0 if a is less
 * than b, >0 if a is > b, and 0 if a == b. */
typedef int (v_array_cmp)(const void *a, const void *b);

/* Array value free callback: free the value v. */
typedef void (v_array_free_cb)(void *v);

v_array *v_array_new(uint sz);
void v_array_append(v_array *a, void *v);
void *v_array_get(v_array *a, uint i);
uint v_array_length(v_array *v);
void v_array_free(v_array *a, v_array_free_cb *cb);

/* Sort the array in place, using the provided comparison callback. */
void v_array_sort(v_array *a, v_array_cmp *cmp);

#endif
