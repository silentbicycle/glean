#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <err.h>
#include <math.h>
#include <sys/time.h>

#include "glean.h"

/* Estimate time remaining based on avg. rate so far, e.g. "1h 20m 14s". */
void print_eta(FILE *f, long sec, long done, long total) {
    struct timeval tv;
    float rate;
    uint rem, h, m, s;
    if (gettimeofday(&tv, NULL) != 0) err(1, "gettimeofday");
    rate = (tv.tv_sec - sec) / (1.0 * done);
    rem = (total - done) * rate;
    h = floor(rem / 3600);
    m = floor((rem % 3600) / 60.0);
    s = floor(rem % 60);
    if (h > 0) fprintf(f, "%dh ", h);
    if (m > 0) fprintf(f, "%dm ", m);
    fprintf(f, "%ds", s);
}
