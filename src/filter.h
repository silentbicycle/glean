#ifndef FILTER_H
#define FILTER_H

int filter_open_coprocess(int *pid);
int filter_match(int fd, char *fname, int len);
void filter_enqueue_files(context *c);

#endif
