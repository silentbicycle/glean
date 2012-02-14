#ifndef ETA_H
#define ETA_H

/* Estimate time remaining based on avg. rate so far, e.g. "1h 20m 14s",
 * assembling it in *buf. Returns the string length, or <0 if it won't fit. */
int eta_tostring(char *buf, int buf_sz, long sec, long done, long total);

/* Print an ETA string to file F. Returns <0 on error. */
int eta_fprintf(FILE *f, long sec, long done, long total);

#endif
