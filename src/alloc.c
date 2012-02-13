#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sys/types.h>

#include "glean.h"

static long allocated = 0;

/* Malloc wrapper for catches failures and profiling allocations.
 * (The tag is used to mark the kind of allocation in the logs.) */
void *alloc(size_t sz, char tag) {
    void *p = malloc(sz);
    if (p == NULL) err(1, "alloc fail");
    if (DEBUG) {
        fprintf(stderr, "-- Allocated %lu bytes (%c), %ld total\n",
            sz, tag, allocated);
        allocated += sz;
    }
    return p;
}
