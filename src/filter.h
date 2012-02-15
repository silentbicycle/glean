#ifndef FILTER_H
#define FILTER_H

/* Open a gln_filter co-process, saving its pid in PID and
 * returning its file descriptor. */
int filter_open_coprocess(int *pid);

/* Read input from the gln_filter co-process and
 * enqueue files to be indexed.
 * Returns <0 on error. */
int filter_enqueue_files(context *c);

#endif
