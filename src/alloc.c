#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include "glean.h"

static long allocated = 0;

/* malloc wrapper for catches failures and profiling allocations */
void *alloc(size_t sz, char tag) {
        void *p = malloc(sz);
        if (p == NULL) err(1, "alloc fail");
        if (DEBUG) {
                fprintf(stderr, "-- Allocated %lu bytes (%c), %ld total\n", sz, tag, allocated);
                allocated += sz;
        }
        return p;
}
