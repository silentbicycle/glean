#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <err.h>
#include <errno.h>

#include "glean.h"
#include "set.h"
#include "word.h"
#include "fname.h"
#include "eta.h"
#include "array.h"
#include "gln_index.h"
#include "worker.h"

/* Start a tokenizer coprocess, setting its stdin & stdout to the socket. */
static int worker_start(int fd, int case_sensitive) {
    char *cs = (case_sensitive ? "-c" : "");
    dup2(fd, 0);            /* set stdin & stdout to full-duplex pipe */
    dup2(fd, 1);
    if (execlp("gln_tokens", "gln_tokens", cs, (char *)NULL) == -1) {
        fprintf(stderr, "worker execlp fail (is gln_tokens in your path?)\n");
        return -1;
    }
    return 0;
}

/* Initialize a worker sub-process. Returns <0 on error. */
static int worker_init(context *c, worker *w) {
    int pair[2], i;
    int res = socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
    if (res == -1) err(1, "socketpair");
    for (i=0; i<2; i++) {
        if (fcntl(pair[0], F_SETFL, O_NONBLOCK) == -1)
            err(1, "setting nonblock failed");
    }
    res = fork();
    if (res == -1) {
        fprintf(stderr, "fork() failure\n");
        return -1;
    } else if (res == 0) {
        if (worker_start(pair[1], c->case_sensitive) < 0)
            return -1;
    } else {
        w->s = pair[0];
        w->fname = NULL;
        w->off = 0;
        w->buf = alloc(sizeof(char) * (BUF_SZ+1), 'b');
    }
    return 0;
}

/* Initialize all worker sub-processes. Returns <0 on error. */
int worker_init_all(context *c) {
    worker *ws = alloc(sizeof(worker) * c->w_ct, 'w');
    uint i, max_sock = 0;
    c->ws = ws;
    c->w_avail = c->w_ct;
    c->w_busy = 0;
    for (i=0; i < c->w_ct; i++) {
        if (worker_init(c, &c->ws[i]) < 0) return -1;
        if (c->ws[i].s > max_sock) max_sock = c->ws[i].s;
    }
    c->max_w_socket = max_sock;
    return 0;
}

/* Try to assign a file to an unoccupied worker, return 1 on success. */
static int assign_file(context *c) {
    fname *fn = (fname *) v_array_get(c->fnames, c->f_ni);
    worker *w;
    uint i, len, len2;
    char fnbuf[PATH_MAX + 1]; /* for add'l \n */
    assert(fn);
    
    /* add a line break, since workers' IO is line-based */
    len = strlen(fn->name);
    snprintf(fnbuf, len + 2, "%s\n", fn->name);
    if (c->verbose) printf(" -- Starting file %s (0x%0x8)\n",
        fn->name, word_hash(fn->name));
    
    for (i=0; i<c->w_ct; i++) {
        w = &c->ws[i];
        if (w->fname == NULL) {
            len2 = write(w->s, fnbuf, len + 1);
            assert(len == len2 - 1);
            w->fname = fn;
            c->f_ni++;
            return 1;
        }
    }
    err(1, "no worker could be assigned");
    return 0;
}

/* Schedule the next enqueud file to an available worker.
 * Returns 1 on success, 0 if complete, or <0 on error. */
int worker_schedule(context *c) {
    int work = -1;
    uint fni = c->f_ni, fnlen = v_array_length(c->fnames);
    if (fni == fnlen) return 0; /* done */
    
    /* Show progress */
    if (c->show_progress && ++c->tick >= c->tick_max) {
        fprintf(stderr, "%.2f%%, %d of %d files, eta ",
            (100.0 * (fni+1)) / fnlen, fni + 1, fnlen);
        eta_fprintf(stderr, c->startsec, fni, fnlen);
        fprintf(stderr, "\n");
        c->tick = 0;
    }
    
    if (assign_file(c)) {
        c->w_avail--;
        c->w_busy++;
        work = 1;               /* successfully scheduled */
    }
    
    return work;
}

/* If word is new, assign token ID and emit "t $tokenid $token\n".
 * Always emit "$tokenid $fileid $data". */
static void note_instance(context *c, worker *w, char *wbuf,
                          uint count, uint len, hash_t fnhash) {
    int known = word_known(c->word_set, wbuf);
    word *word = NULL;
    if (known) {
        word = word_get(c->word_set, wbuf);
        assert(strcmp(wbuf, word->name) == 0);
        word->count += count;
        c->t_occ_ct += count;
    } else {
        word = word_add(c->word_set, wbuf, len);
        c->t_ct++;
    }        
    if (word) {
        h_array_append(word->a, fnhash);
    } else {
        fprintf(stderr, "Failed to allocate word\n");
        exit(EXIT_FAILURE);
    }
    if (c->verbose > 1) printf("GOT: %s (%d) in %04x, %d\n",
        wbuf, len, fnhash, count);
}

static int digits(uint d) {
    uint r=0;
    while (d >= 10) { r++; d /= 10; }
    if (d > 0) r++;
    return r;
}

/* Handle data read from a worker. */
static void process_read(context *c, worker *w, int len, int wid) {
    int i, toks=0, last=0;
    char wbuf[MAX_WORD_SZ];
    char *in = w->buf;
    uint count;
    uint wlen;      /* current word's length */
    hash_t fnhash = word_hash(w->fname->name);
    if (DEBUG)
        fprintf(stderr, "Current fname: %s -> %04x\n", w->fname->name, fnhash);
    
    for (i=0; i<len; i++) {
        if (in[i] == '\n') {
            toks = sscanf(in + last, "%s %u\n", wbuf, &count);
            if (toks == 2) {
                wlen = i - last - 1 - digits(count);
                assert(wlen == strlen(wbuf));
                note_instance(c, w, wbuf, count, wlen, fnhash);
                last = i + 1;
            } else if (strncmp(in + last, " SKIP", 5) == 0) {
                if (c->verbose >= 1) printf(" -- Skipping file %s\n", w->fname->name);
                w->fname = NULL; w->off = 0;
                c->w_busy--; c->w_avail++;
                return;                                
            } else if (strncmp(in + last, " DONE", 5) == 0) {
                if (c->verbose > 1) printf(" -- Done with file %s\n", w->fname->name);
                fname_add(c->fn_set, w->fname);
                w->fname = NULL; w->off = 0;
                c->w_busy--; c->w_avail++;
                return;
            } else {
                err(1, "printf-debugging corrupted worker input?");
            }
        }
    }
    
    w->off = len - last;
    if (DEBUG) printf("Copying remaining %d of %d (last:%d): %s\n",
        w->off, len, last, in + last);
    strncpy(in, in + last, w->off + 1); /* strlcpy */
    /* strcpy(in, in + last); */
    if (DEBUG) printf(" ==== Copied -- \n%s\n====\n", in);
    assert(in[0] != '\n');
}

/* Check worker sub-processes and and process input, if any. */
int worker_check(context *c, fd_set *fdsr) {
    int i, len=0;
    int ready_ct;
    struct timeval tv = { 0, 10 * 1000 };
    worker *w;
    
    for (i=0; i<c->w_ct; i++) FD_SET(c->ws[i].s, fdsr);
    ready_ct = select(c->max_w_socket + 1, fdsr, NULL, NULL, &tv);
    if (ready_ct == -1) err(1, "select");
    
    /* check if any workers are done, read + log data */
    for (i=0; i<c->w_ct; i++) {
        w = &c->ws[i];
        if (FD_ISSET(w->s, fdsr)) {
            do {
                len = read(w->s, w->buf + w->off, BUF_SZ - w->off);
                if (len > 0) {
                    w->buf[len + w->off] = '\0';
                    process_read(c, w, len + w->off, i);
                } else if (len == 0) {
                    printf("EOF'd %d; %s\n", i, w->buf);
                    /* EOF? */
                } else if (errno == EAGAIN) {
                    errno = 0;
                    break;
                } else {
                    err(1, "worker read fail");
                }
            } while (len > 0);
        }
    }
    return 0;
}
