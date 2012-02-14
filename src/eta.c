#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <err.h>
#include <math.h>
#include <sys/time.h>

#include "glean.h"

/* Estimate time remaining based on avg. rate so far, e.g. "1h 20m 14s",
 * assembling it in *buf. Returns the string length, or <0 if it won't fit. */
int eta_tostring(char *buf, int buf_sz, long sec, long done, long total) {
    struct timeval tv;
    float rate;
    uint rem, h, m, s;
    int bo = 0, len = 0;                /* buffer offset, length */
    if (gettimeofday(&tv, NULL) != 0) err(1, "gettimeofday");
    rate = (tv.tv_sec - sec) / (1.0 * done);
    rem = (total - done) * rate;
    h = floor(rem / 3600);
    m = floor((rem % 3600) / 60.0);
    s = floor(rem % 60);

    if (h > 0) {
        len = snprintf(buf + bo, buf_sz - bo, "%dh ", h);
        if (len >= buf_sz - bo) return -1;
        bo += len;
    }
    if (m > 0) {
        len = snprintf(buf + bo, buf_sz - bo, "%dm ", m);
        if (len >= buf_sz - bo) return -1;
        bo += len;
    }
    len = snprintf(buf + bo, buf_sz - bo, "%ds ", s);
    if (len >= buf_sz - bo) return -1;
    bo += len;
    return bo;
}

/* Print an ETA string to file F. */
int eta_fprintf(FILE *f, long sec, long done, long total) {
    char buf[1024];
    if (eta_tostring(buf, 1024, sec, done, total) < 0) return -1;
    fprintf(f, buf);
    return 0;
}
