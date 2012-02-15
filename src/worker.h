#ifndef WORKER_H
#define WORKER_H

/* Initialize a worker sub-process. Returns <0 on error. */
int worker_init_all(context *c);

/* Schedule the next enqueud file to an available worker.
 * Returns 1 on success, 0 if complete, or <0 on error. */
int worker_schedule(context *c);

/* Check worker sub-processes and and process input, if any. */
int worker_check(context *c, fd_set *fdsr);

#endif
