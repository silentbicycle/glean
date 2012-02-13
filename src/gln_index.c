#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <signal.h>

#include "glean.h"
#include "hash.h"
#include "whash.h"
#include "fhash.h"
#include "tokenize.h"
#include "array.h"
#include "gln_index.h"
#include "db.h"
#include "stopword.h"
#include "eta.h"
#include "nextline.h"

static void usage() {
    puts("usage: gln_index [-hVvpcs] [-d DB_DIR] [-r INDEX_ROOT] \n"
        "[-f FILTER_CONFIG_FILE] [-w WORKER_CT]");
    exit(1);
}

static void version() {
    int i;
    char *v = GLN_VERSION_STRING;
    printf("gln_index, version ");
    for (i=0; i<3; i++) {
        if (v[2*i] != '0') printf("%c", v[2*i]);
        printf("%c", v[2*i+1]);
        if (i < 2) printf(".");
    }
    printf(" by Scott Vokes\n");
    exit(0);
}

/* Start a tokenizer coprocess, setting its stdin & stdout to the socket. */
static void start_worker(int fd, int case_sensitive) {
    char *cs = (case_sensitive ? "-c" : "");
    dup2(fd, 0);            /* set stdin & stdout to full-duplex pipe */
    dup2(fd, 1);
    if (execlp("gln_tokens", "gln_tokens", cs, (char *)NULL) == -1)
        err(1, "worker execlp fail (is gln_tokens in your path?)");
}

/* Initialize an individual worker process. */
static void init_worker(context *c, worker *w) {
    int pair[2], i;
    int res = socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
    if (res == -1) err(1, "socketpair");
    for (i=0; i<2; i++) {
        if (fcntl(pair[0], F_SETFL, O_NONBLOCK) == -1)
            err(1, "setting nonblock failed");
    }
    res = fork();
    if (res == -1) {
        err(1, "fork");
    } else if (res == 0) {
        start_worker(pair[1], c->case_sensitive);
    } else {
        w->s = pair[0];
        w->fname = NULL;
        w->off = 0;
        w->buf = alloc(sizeof(char) * (BUF_SZ+1), 'b');
    }
}

static char *get_gln_path(context *c, const char *fname) {
    int len = strlen(c->wkdir) + strlen(fname) + 2;
    char *path = alloc(len, 'p'); /* could use alloca... */
    snprintf(path, len + 1, "%s/%s", c->wkdir, fname);
    return path;
}

static FILE *open_gln_log(context *c, const char *fname) {
    FILE *log;
    char *path = get_gln_path(c, fname);
    if (DEBUG) fprintf(stderr, "opening file %s\n", path);
    log = fopen(path, "w");
    if (log == NULL) err(1, "failed to open log file");
    free(path);
    return log;
}

static int open_db(char *path, char *file, int update) {
    int plen = strlen(path), flen = strlen(file);
    char *fn;
    int fd;
    assert(plen > 0); assert(flen > 0);
    plen += strlen(file) + 2;
    fn = alloc(plen, 'f');
    snprintf(fn, plen, "%s/%s", path, file);
    fd = open(fn, O_RDWR | (update ? 0 : O_TRUNC) | O_CREAT /* sic */, 0744);
    if (DEBUG) fprintf(stderr, "opening %s, got fd %d\n", fn, fd);
    if (fd == -1) err(1, "%s", fn);
    free(fn);
    return fd;
}

static context *init_context() {
    context *c = alloc(sizeof(context), 'c');
    c->wkdir = default_gln_dir();
    c->root = NULL;
    
    c->wt = init_word_table(0);
    c->ft = init_fname_table(0);
    c->verbose = c->show_progress = c->use_stop_words = 0;
    c->case_sensitive = 0;
    c->index_dotfiles = 0;
    c->update = 0;
    c->compressed = 0;
    c->tct = c->toct = 0;
    c->fni = c->tick = c->tick_max = 0;
    c->fnames = v_array_init(16);
    c->w_ct = DEF_WORKER_CT;
    return c;
}

static const char *find_cmd_fmt = "find %s -type f";

static int fsize(int fd) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) err(1, "fstat");
    return sb.st_size;
}

static int open_filter_coprocess(int *pid) {
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

static void init_files(context *c) {
    FILE *finder;
    int tstamp, gln_path_len = strlen(c->wkdir) + strlen("/.gln/");
    char *gln_path = alloc(gln_path_len, 'p');
    char *find_cmd = NULL;
    char *cwd = NULL;
    int find_cmd_len;
    
    snprintf(gln_path, gln_path_len, "%s/.gln/", c->wkdir);
    if (mkdir(gln_path, 0755) != 0) {
        if (errno != EEXIST) err(1, "failed to create working dir");
        errno = 0;
    }
    
    if (c->root == NULL) {
        c->root = getcwd(NULL, MAXPATHLEN);
    } else {
        cwd = getcwd(NULL, MAXPATHLEN);
        if (chdir(c->root) == -1) err(1, "root dir");
        c->root = getcwd(NULL, MAXPATHLEN);
    }
    
    find_cmd_len = strlen(find_cmd_fmt) - 1 + strlen(c->root);
    find_cmd = alloc(find_cmd_len, 'C');
    snprintf(find_cmd, find_cmd_len, find_cmd_fmt, c->root);
    if ((finder = popen(find_cmd, "r")) == NULL) err(1, "finder fail");
    c->find = finder;
    if (cwd) {
        if (chdir(cwd) == -1) err(1, "chdir-ing back");
        free(cwd);
    }
    
    c->filter_fd = open_filter_coprocess(&c->filter_pid);
    c->tlog = open_gln_log(c, ".gln/tokens");
    c->settings = open_gln_log(c, ".gln/settings");
    c->swlog = open_gln_log(c, ".gln/stopwords");
    
    c->fdb_fd = open_db(c->wkdir, ".gln/fname.db", c->update);
    c->tdb_fd = open_db(c->wkdir, ".gln/token.db", c->update);
    if (fsize(c->fdb_fd) == 0) init_db_files(c);
    
    tstamp = open_db(c->wkdir, ".gln/timestamp", 0);
    if (close(tstamp) == -1) err(1, "Couldn't write timestamp file");
}

static void save_settings(context *c) {
    fprintf(c->settings, "case_sensitive %d\n", c->case_sensitive);
    fprintf(c->settings, "compressed %d\n", c->compressed);
    /* other options go here later */
}

static void init_workers(context *c) {
    worker *ws = alloc(sizeof(worker) * c->w_ct, 'w');
    uint i, max_sock = 0;
    c->ws = ws;
    c->w_avail = c->w_ct;
    c->w_busy = 0;
    for (i=0; i < c->w_ct; i++) {
        init_worker(c, &c->ws[i]);
        if (c->ws[i].s > max_sock) max_sock = c->ws[i].s;
    }
    c->max_w_socket = max_sock;
}

/* Try to assign a file to an unoccupied worker, return 1 on success. */
static int assign_file(context *c) {
    fname *fn = (fname *)v_array_get(c->fnames, c->fni);
    worker *w;
    uint i, len, len2;
    char fnbuf[PATH_MAX + 1]; /* for add'l \n */
    assert(fn);
    
    /* add a line break, since workers' IO is line-based */
    len = strlen(fn->name);
    snprintf(fnbuf, len + 2, "%s\n", fn->name);
    if (c->verbose) printf(" -- Starting file %s (0x%0x8)\n", fn->name, hash_word(fn->name));
    
    for (i=0; i<c->w_ct; i++) {
        w = &c->ws[i];
        if (w->fname == NULL) {
            len2 = write(w->s, fnbuf, len + 1);
            assert(len == len2 - 1);
            w->fname = fn;
            c->fni++;
            return 1;
        }
    }
    err(1, "no worker could be assigned");
    return 0;
}

static void free_context(context *c) {
    int i;
    worker *w;
    table_free(c->wt, free_word);
    table_free(c->ft, free_fname);
    for (i=0; i<c->w_ct; i++) {
        w = &(c->ws[i]);
        free(w->buf);
    }
    
    free(c->ws);
    
    if (fclose(c->tlog) != 0 || fclose(c->swlog) != 0 || fclose(c->settings) != 0)
        err(1, "fclose failed");
    if ((close(c->fdb_fd) == -1) || (close(c->tdb_fd) == -1))
        err(1, "close");
}

static int gzip_tokens_file(context *c) {
    char *tf = get_gln_path(c, ".gln/tokens");
    char *buf = alloc(BUF_SZ, 'b');
    int res, len = strlen(tf);
    snprintf(buf, 3*len + 32, "sort \"%s\" | gzip > \"%s.gz\" && rm \"%s\"",
        tf, tf, tf);
    res = system(buf);
    free(tf);
    free(buf);
    return res;
}

static int finish(context *c) {
    int i, res;
    /* These should be written to .gln_new/totals */
    
    /* cull child processes */
    for (i=0; i<c->w_ct; i++) {
        write(c->ws[i].s, " DONE\n", 6);
        if (DEBUG) puts(" -- Sending child DONE");
    }
    
    if (c->use_stop_words) {
        if (c->show_progress) fprintf(stderr, "-- Analyzing stop words\n");
        identify_stop_words(c);
    }
    if (c->show_progress) fprintf(stderr, "-- Writing databases\n");
    res = write_db(c);
    
    if (c->verbose)
        fprintf(stderr, "%lu tokens, %u files\n", c->tct, c->fni);
    
    if (res == 0 && c->compressed) res = gzip_tokens_file(c);
    
    free_context(c);
    free(c);
    free_nextline_buffer();
    return res;               /* for now */
}

static int filter_match(int fd, char *fname, int len) {
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

static void enqueue_files(context *c) {
    char *buf;
    size_t len;
    fname *fn;
    uint ct=0, sp = (c->verbose || c->show_progress);
    int matched;
    if (c->find == NULL) return;
    
    if (sp) fprintf(stderr, "-- enqueueing all files to index\n");
    
    while ((buf = nextline(c->find, &len)) != NULL) {
        matched = filter_match(c->filter_fd, buf, len);
        if (buf[len - 1] == '\n') buf[len - 1] = '\0';
        if (matched) {
            if (c->verbose || DEBUG)
                fprintf(stderr, "Ignoring: %s\n", buf);
        } else {
            fn = new_fname(buf, len);
            v_array_append(c->fnames, fn);
            if (c->verbose > 1) fprintf(stderr, "Appended: %d %s\n",
                v_array_length(c->fnames), fn->name);
            ct++;
            if (ct % ENQUEUE_PROGRESS_CT == 0)
                fprintf(stderr, "%u files enqueued...\n", ct);
        }
    }
    
    /* clean up find & filter processes */
    if (close(c->filter_fd) == -1) err(1, "close");
    if ((kill(c->filter_pid, SIGKILL)) == -1) err(1, "kill");
    if ((wait(NULL) == -1)) err(1, "wait");
    c->find = NULL;
    
    c->tick_max = v_array_length(c->fnames) / 100;
    
    if (sp) fprintf(stderr, "-- enqueueing done, indexing %u files\n", ct);
}

static int schedule_workers(context *c) {
    int work=0;
    uint fni = c->fni, fnlen = v_array_length(c->fnames);
    if (fni == fnlen) return 0;
    
    /* Show progress */
    if (c->show_progress && ++c->tick >= c->tick_max) {
        fprintf(stderr, "%.2f%%, %d of %d files, eta ",
            (100.0 * (fni+1)) / fnlen, fni + 1, fnlen);
        print_eta(stderr, c->startsec, fni, fnlen);
        fprintf(stderr, "\n");
        c->tick = 0;
    }
    
    if (assign_file(c)) {
        c->w_avail--;
        c->w_busy++;
        work = 1;
    }
    
    return work;
}

static void cons_word(context *c, word *w, hash_t hash) {
    assert(w);
    assert(w->a);
    h_array_append(w->a, hash);
    if (HASH_BYTES == 2) assert(hash == hash % 0xffff);
}

/* If word is new, assign token ID and emit "t $tokenid $token\n".
 * Always emit "$tokenid $fileid $data". */
static void note_instance(context *c, worker *w, char *wbuf,
    uint data, uint len, hash_t fnhash) {
    int known = known_word(c->wt, wbuf);
    word *word = NULL;
    if (known) {
        word = get_word(c->wt, wbuf);
        assert(strcmp(wbuf, word->name) == 0);
        word->i += data;
        c->toct += data;
    } else {
        word = add_word(c->wt, wbuf, len);
        c->tct++;
    }        
    /* cons word */
    if (word) cons_word(c, word, fnhash);
    if (c->verbose > 1) printf("GOT: %s (%d) in %04x, %d\n", wbuf, len, fnhash, data);
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
    uint data;
    uint wlen;      /* current word's length */
    hash_t fnhash = hash_word(w->fname->name);
    if (DEBUG)
        fprintf(stderr, "Current fname: %s -> %04x\n", w->fname->name, fnhash);
    
    for (i=0; i<len; i++) {
        if (in[i] == '\n') {
            toks = sscanf(in + last, "%s %u\n", wbuf, &data);
            if (toks == 2) {
                wlen = i - last - 1 - digits(data);
                assert(wlen == strlen(wbuf));
                note_instance(c, w, wbuf, data, wlen, fnhash);
                last = i + 1;
            } else if (strncmp(in + last, " SKIP", 5) == 0) {
                if (c->verbose >= 1) printf(" -- Skipping file %s\n", w->fname->name);
                w->fname = NULL; w->off = 0;
                c->w_busy--; c->w_avail++;
                return;                                
            } else if (strncmp(in + last, " DONE", 5) == 0) {
                if (c->verbose > 1) printf(" -- Done with file %s\n", w->fname->name);
                add_fname(c->ft, w->fname);
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

static int check_workers(context *c, fd_set *fdsr) {
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

static void handle_args(context *c, int *argc, char **argv[]) {
    int f, iarg;
    while ((f = getopt(*argc, *argv, "hVvpcCud:r:w:f:s")) != -1) {
        switch (f) {
        case 'h':       /* help */
            usage();
            break;
        case 'v':       /* verbose */
            c->verbose++;
            if (c->verbose > 1 &&
                setenv("GLN_FILTER_DEBUG", "1", 1) == -1)
                err(1, "setenv");
            break;
#if NYI
        case 'u':       /* update existing db */
            c->update = 1;
            break;
#endif
        case 'p':       /* show progress */
            c->show_progress = 1;
            break;
        case 'c':       /* case-sensitive */
            c->case_sensitive = 1;
            break;
        case 'C':       /* compress tokens file */
            c->compressed = 1;
            break;
        case 'd':       /* DB dir */
            c->wkdir = (strcmp(optarg, ".") == 0 ? 
                getcwd(NULL, MAXPATHLEN) : optarg);
            break;
        case 'r':       /* root dir of index */
            c->root = (strcmp(optarg, ".") == 0 ? 
                getcwd(NULL, MAXPATHLEN) : optarg);
            break;
        case 'w':       /* worker count */
            iarg = atoi(optarg);
            if (iarg < 1 || iarg > MAX_WORKER_CT) {
                fprintf(stderr, "Invalid worker count: %d\n", iarg);
                exit(1);
            }
            c->w_ct = iarg;
            break;
        case 'f':       /* set filtering config. file */
            if ((setenv("GLN_FILTER_FILE", optarg, 1)) == -1)
                err(1, "setenv");
            break;
        case 's':       /* ID and filter stop words */
            c->use_stop_words = 1;
            break;
        case 'V':
            version();
            /* NOTREACHED */
        default:
            usage();
            /* NOTREACHED */
        }
    }
    *argc -= optind;
    *argv += optind;
}

int main(int argc, char *argv[]) {
    context *c = init_context();
    fd_set *fdsr = NULL;
    struct timeval tv;
    int i, action=0;
    uint fnlen;
    
    handle_args(c, &argc, &argv);
    init_files(c);
    save_settings(c);
    
    enqueue_files(c);
    init_workers(c);
    
    fnlen = v_array_length(c->fnames);
    
    fdsr = (fd_set *) alloc(sizeof(fd_set), 's');
    
    if (gettimeofday(&tv, NULL) != 0) err(1, "gettimeofday");
    c->startsec = tv.tv_sec;
    tv.tv_sec = 0;
    tv.tv_usec = 10 * 1000;
    
    for (;;) {
        assert(tv.tv_sec == 0);
        assert(c->w_busy + c->w_avail == c->w_ct);
        if (c->w_avail > 0) action = schedule_workers(c);
        if (c->w_busy > 0) action = check_workers(c, fdsr) || action;
        
        if (action) {
            action = 0;
        } else if (c->fni == fnlen && c->w_busy == 0) {
            free(fdsr);
            return finish(c);
        } else {
            select(0, NULL, NULL, NULL, &tv); /* don't busyloop */
            if (DEBUG) {
                fprintf(stderr, "pipe done... w_avail: %d, w_ct: %d\n",
                    c->w_avail, c->w_ct);
            }
        }
        
        if (DEBUG) {
            printf("-- Working:");
            for (i=0; i<c->w_ct; i++)
                if (c->ws[i].fname != NULL) printf(" %d", i);
            printf(" - idle: %d\n", c->w_avail);
        }
    }
}
