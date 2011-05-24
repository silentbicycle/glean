#ifndef OCC_ARRAY_H
#define OCC_ARRAY_H

/* resizable uint -> hash_t array.
 * Separate since sizeof(hash_t) is much less than sizeof(void *),
 * and most live data is hash_t arrays - major memory savings. */
typedef struct h_array {
        uint sz;                /* allocated size */
        uint len;               /* filled length */
        hash_t *hs;             /* file hashes */
} h_array;

h_array *h_array_init(uint sz);
void h_array_append(h_array *a, hash_t v);
hash_t h_array_get(h_array *a, uint i);
uint h_array_length(h_array *a);
void h_array_sort(h_array *a);
void h_array_uniq(h_array *a);
h_array *h_array_union(h_array *a, h_array *b);
h_array *h_array_intersection(h_array *a, h_array *b);
h_array *h_array_complement(h_array *a, h_array *b);
void h_array_free(h_array *a);

typedef struct v_array {
        uint sz;                /* allocated size */
        uint len;               /* filled length */
        void **vs;              /* values */
} v_array;

v_array *v_array_init(uint sz);
void v_array_append(v_array *a, void *v);
void *v_array_get(v_array *a, uint i);
uint v_array_length(v_array *v);
void v_array_sort(v_array *a, int (*cmp)(const void *, const void *));
void v_array_free(v_array *a, void (*free_val)(void *));

#endif
