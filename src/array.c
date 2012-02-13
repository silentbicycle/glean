#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>

#include "glean.h"
#include "array.h"

/**************
 * Hash array *
 **************/

h_array *h_array_init(uint sz) {
    h_array *a = alloc(sizeof(h_array), 'H');
    hash_t *hs = alloc(sz * sizeof(hash_t), 'h');
    assert(sz > 0);
    a->sz = sz;
    a->len = 0;
    a->hs = hs;
    return a;
}

static void h_array_resize(h_array *a) {
    uint i, nsz = 2*a->sz;
    hash_t *hs = alloc(nsz * sizeof(hash_t), 'H');
    assert(hs); assert(a);
    assert(a->hs);
    assert(a->len < nsz);
    for (i=0; i<a->len; i++) hs[i] = a->hs[i];
    free(a->hs);
    a->hs = hs;
    a->sz = nsz;
}

void h_array_append(h_array *a, hash_t v) {
    assert(a);
    if (a->len + 1 >= a->sz) h_array_resize(a);
    a->hs[a->len++] = v;
}

uint h_array_length(h_array *a) { assert(a); return a->len; }

hash_t h_array_get(h_array *a, uint i) { assert(a); return a->hs[i]; }

static int cmp_hash(const void *va, const void *vb) {
    hash_t a = *((hash_t *) va), b = *((hash_t *) vb);
    return (a < b ? -1 : a > b ? 1 : 0);
}

void h_array_sort(h_array *a) {
    qsort(a->hs, a->len, sizeof(hash_t), cmp_hash);
}

/* Remove duplicate values. Assumes array is sorted. */
void h_array_uniq(h_array *a) {
    uint i, dup=0;
    hash_t last = 0, cur;
    hash_t *curhs = a->hs, *nhs = alloc(a->sz * sizeof(hash_t), 'H');
    for (i=0; i<a->len; i++) {
        cur = a->hs[i];
        if (i > 0 && last == cur) {
            dup++;
        } else {
            nhs[i - dup] = cur;
        }
        assert(last <= cur);
        last = cur;
    }
    a->len -= dup;
    free(curhs);
    a->hs = nhs;
}

h_array *h_array_union(h_array *a, h_array *b) {
    h_array *z;
    uint ia = 0, ib = 0;
    uint lena = h_array_length(a), lenb = h_array_length(b);
    hash_t ha, hb;
    if (lena == 0) {
        z = b;
    } else if (lenb == 0) {
        z = a;
    } else {                
        z = h_array_init(2);
        while (1) {
            ha = h_array_get(a, ia);
            hb = h_array_get(b, ib);
            if (ha == hb) {
                h_array_append(z, ha);
                ia++; ib++;
            } else if (ha < hb) {
                h_array_append(z, ha);
                ia++;
            } else /* ha > hb */ {
                h_array_append(z, hb);
                ib++;
            }
            if (ia >= lena) {
                while (ib < lenb)
                    h_array_append(z, h_array_get(b, ib++));
                break;
            } else if (ib >= lenb) {
                while (ia < lena)
                    h_array_append(z, h_array_get(a, ia++));
                break;
            }
        }
    }
    assert(z);
    assert(h_array_length(z) >= lena && h_array_length(z) >= lenb);
    return z;
}

h_array *h_array_intersection(h_array *a, h_array *b) {
    h_array *z;
    uint ia = 0, ib = 0;
    uint lena = h_array_length(a), lenb = h_array_length(b);
    hash_t ha, hb;
    z = h_array_init(2);
    if (lena > 0 && lenb > 0) {
        while (1) {
            ha = h_array_get(a, ia);
            hb = h_array_get(b, ib);
            if (ha == hb) {
                h_array_append(z, ha);
                ia++; ib++;
            } else if (ha < hb) {
                ia++;
            } else /* ha > hb */ {
                ib++;
            }
            if (ia >= lena || ib >= lenb) break;
        }
    }
    assert(z);
    assert(h_array_length(z) <= lena && h_array_length(z) <= lenb);
    return z;
}

h_array *h_array_complement(h_array *a, h_array *b) {
    h_array *z;
    uint ia = 0, ib = 0;
    uint lena = h_array_length(a), lenb = h_array_length(b);
    hash_t ha, hb;
    if (lenb == 0) {
        z = a;
    } else if (lena == 0) {
        z = h_array_init(2);
    } else {
        z = h_array_init(2);
        while (1) {
            ha = h_array_get(a, ia);
            hb = h_array_get(b, ib);
            if (ha == hb) {
                ia++; ib++;
            } else if (ha < hb) {
                h_array_append(z, ha);
                ia++;
            } else {        /* ha > hb */
                ib++;
            }
            if (ia >= lena) {
                break;
            } else if (ib >= lenb) {
                while (ia < lena)
                    h_array_append(z, h_array_get(a, ia++));
                break;
            }
        }
    }
    assert(z);
    assert(h_array_length(z) <= lena);
    return z;
}

void h_array_free(h_array *a) {
    if (a->hs) free(a->hs);
    free(a);
}


/****************
 * Void * array *
 ****************/

v_array *v_array_init(uint sz) {
    v_array *a = alloc(sizeof(v_array), 'v');
    void **vs = alloc(sz * sizeof(void *), 'v');
    assert(sz > 0);
    a->sz = sz;
    a->len = 0;
    a->vs = vs;
    return a;
}

static void v_array_resize(v_array *a) {
    uint i, nsz = 2*a->sz;
    void **vs = alloc(nsz * sizeof(void *), 'v');
    assert(vs); assert(a);
    assert(a->vs);
    assert(a->len < nsz);
    for (i=0; i<a->len; i++) vs[i] = a->vs[i];
    free(a->vs);
    a->vs = vs;
    a->sz = nsz;
}

void v_array_append(v_array *a, void *v) {
    assert(a);
    if (a->len + 1 >= a->sz) v_array_resize(a);
    a->vs[a->len++] = v;
}

uint v_array_length(v_array *a) { return a->len; }

void *v_array_get(v_array *a, uint i) { assert(a); return a->vs[i]; }

void v_array_sort(v_array *a, v_array_cmp *cmp) {
    qsort(a->vs, a->len, sizeof(void *), cmp);
}

void v_array_free(v_array *a, v_array_free_cb *cb) {
    uint i;
    assert(a->len <= a->sz);
    if (cb) {
        for (i=0; i<a->len; i++) cb(a->vs[i]);
    }
    free(a->vs);
    free(a);
}
