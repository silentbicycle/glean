#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <err.h>
#include <errno.h>
#include <signal.h>

#include "glean.h"
#include "set.h"
#include "fname.h"
#include "array.h"
#include "gln_index.h"
#include "nextline.h"

/* Open a gln_filter co-process, saving its pid in PID and
 * returning its file descriptor. */
int filter_open_coprocess(int *pid) {
    int res, sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) err(1, "socketpair");
    res = fork();
    if (res == -1) {
        err(1, "fork");
    } else if (res == 0) {  /* child */
        dup2(sv[1], STDIN_FILENO),
          dup2(sv[1], STDOUT_FILENO);
        if (execlp("gln_filter", "gln_filter", (char *)NULL) == -1)
            err(1, "filter execlp fail (is gln_filter in your path?");
    }
    *pid = res;
    return sv[0];
}

/* Should a given file be skipped, according to filtering rules? */
static int should_skip(int fd, char *fname, int len) {
    char buf[4096];
    int sz;
    strncpy(buf, fname, len + 1);
    buf[len-1] = '\n'; buf[len] = '\0';    /* for getline */
    sz = write(fd, buf, len);
    assert(sz > 0);
    sz = read(fd, buf, 4095);
    assert(sz > 0);
    buf[sz] = '\0';
    if (strncmp(buf, "ignore", sz - 1) == 0) return 1;
    if (strncmp(buf, "index", sz - 1) == 0) return 0;
    fprintf(stderr, "Bad filter response: %s\n"
        "currently only 'ignore' and 'index' are supported.\n", buf);
    return 1;
}

/* Read input from the gln_filter co-process and
 * enqueue files to be indexed.
 * Returns <0 on error. */
int filter_enqueue_files(context *c) {
    char *buf;
    size_t len;
    fname *fn;
    uint ct=0, sp = (c->verbose || c->show_progress);
    int skip = 0;
    if (c->find == NULL) {
        fprintf(stderr, "Failed to start 'find' process.\n");
        return -1;
    }
    
    if (sp) fprintf(stderr, "-- Enqueueing all files to index...\n");
    
    while ((buf = nextline(c->find, &len)) != NULL) {
        skip = should_skip(c->filter_fd, buf, len);
        if (buf[len - 1] == '\n') buf[len - 1] = '\0';
        if (skip) {
            if (c->verbose || DEBUG)
                fprintf(stderr, "Ignoring: %s\n", buf);
        } else {
            fn = fname_new(buf, len);
            v_array_append(c->fnames, fn);
            if (c->verbose > 1) fprintf(stderr, "Appended: %d %s\n",
                v_array_length(c->fnames), fn->name);
            ct++;
            if (ct % ENQUEUE_PROGRESS_CT == 0)
                fprintf(stderr, "%u files enqueued so far...\n", ct);
        }
    }
    
    /* clean up find & filter processes */
    if (close(c->filter_fd) == -1) err(1, "close");
    if ((kill(c->filter_pid, SIGKILL)) == -1) err(1, "kill");
    if ((wait(NULL) == -1)) err(1, "wait");
    c->find = NULL;
    
    c->tick_max = v_array_length(c->fnames) / 100;
    
    if (sp) fprintf(stderr, "-- %u files enqueued.\n", ct);
    return 0;
}
