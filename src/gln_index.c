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

#include "glean.h"
#include "set.h"
#include "word.h"
#include "fname.h"
#include "array.h"
#include "gln_index.h"
#include "db.h"
#include "stopword.h"
#include "filter.h"
#include "nextline.h"
#include "worker.h"

static void usage() {
    fprintf(stderr,
        "usage: gln_index [-hVvpcs] [-d DB_DIR] [-r INDEX_ROOT] \n"
        "                 [-f FILTER_CONFIG_FILE] [-w WORKER_CT]\n"
        "    See gln_filter(1) for more information.\n");
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

static char *get_gln_path(context *c, const char *fname) {
    int len = strlen(c->wkdir) + strlen(fname) + 2;
    char *path = alloc(len, 'p'); /* could use alloca... */
    if (len + 1 <= snprintf(path, len + 1,
            "%s/%s", c->wkdir, fname)) {
        fprintf(stderr, "snprintf error\n");
        exit(EXIT_FAILURE);
    }
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
    if (plen <= snprintf(fn, plen, "%s/%s", path, file)) {
        fprintf(stderr, "snprintf error\n");
        exit(EXIT_FAILURE);
    }
    fd = open(fn, O_RDWR | (update ? 0 : O_TRUNC) | O_CREAT /* sic */, 0744);
    if (DEBUG) fprintf(stderr, "opening %s, got fd %d\n", fn, fd);
    if (fd == -1) err(1, "%s", fn);
    free(fn);
    return fd;
}

static context *init_context() {
    context *c = alloc(sizeof(context), 'c');
    c->wkdir = db_default_gln_dir();
    c->root = NULL;
    
    c->word_set = word_set_init(0);
    c->fn_set = fname_new_set(0);
    c->verbose = c->show_progress = c->use_stop_words = 0;
    c->case_sensitive = 0;
    c->index_dotfiles = 0;
    c->update = 0;
    c->compressed = 0;
    c->t_ct = c->t_occ_ct = 0;
    c->f_ni = c->tick = c->tick_max = 0;
    c->fnames = v_array_new(16);
    c->w_ct = DEF_WORKER_CT;
    c->w_offset = 0;
    return c;
}

static const char *find_cmd_fmt = "find %s -type f";

static int fsize(int fd) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) err(1, "fstat");
    return sb.st_size;
}

static void init_files(context *c) {
    FILE *finder;
    int tstamp, gln_path_len = strlen(c->wkdir) + strlen("/.gln/") + 1;
    char *gln_path = alloc(gln_path_len, 'p');
    char *find_cmd = NULL;
    char *cwd = NULL;
    int find_cmd_len;
    
    if (gln_path_len <= snprintf(gln_path, gln_path_len,
            "%s/.gln/", c->wkdir)) {
        fprintf(stderr, "snprintf error\n");
        exit(EXIT_FAILURE);
    }
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
    if (find_cmd_len <= snprintf(find_cmd, find_cmd_len,
            find_cmd_fmt, c->root)) {
        fprintf(stderr, "snprintf error\n");
        exit(EXIT_FAILURE);
    }
    if ((finder = popen(find_cmd, "r")) == NULL) err(1, "finder fail");
    c->find = finder;
    if (cwd) {
        if (chdir(cwd) == -1) err(1, "chdir-ing back");
        free(cwd);
    }
    
    c->filter_fd = filter_open_coprocess(&c->filter_pid);
    c->tlog = open_gln_log(c, ".gln/tokens");
    c->settings = open_gln_log(c, ".gln/settings");
    c->swlog = open_gln_log(c, ".gln/stopwords");
    
    c->fdb_fd = open_db(c->wkdir, ".gln/fname.db", c->update);
    c->tdb_fd = open_db(c->wkdir, ".gln/token.db", c->update);
    if (fsize(c->fdb_fd) == 0) db_init_files(c);
    
    tstamp = open_db(c->wkdir, ".gln/timestamp", 0);
    if (close(tstamp) == -1) err(1, "Couldn't write timestamp file");
}

static void save_settings(context *c) {
    fprintf(c->settings, "case_sensitive %d\n", c->case_sensitive);
    fprintf(c->settings, "compressed %d\n", c->compressed);
    /* other options go here later */
}

static void free_context(context *c) {
    int i;
    worker *w;
    set_free(c->word_set, word_free);
    set_free(c->fn_set, fname_free_cb);
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

/* Execute `sort FILE | gzip > FILE.gz && rm FILE`. */
static int gzip_tokens_file(context *c) {
    char *tf = get_gln_path(c, ".gln/tokens");
    int res, len = strlen(tf);
    char *fmt = "sort \"%s\" | gzip > \"%s.gz\" && rm \"%s\"";
    int buf_sz = 3*len + strlen(fmt) + 1;
    char *buf = alloc(buf_sz, 'b');
    if (buf_sz <= snprintf(buf, buf_sz,
            fmt, tf, tf, tf)) {
        fprintf(stderr, "snprintf error\n");
        exit(EXIT_FAILURE);
    }
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
        if (stopword_identify(c->word_set, c->t_ct, c->t_occ_ct, c->verbose) < 0) {
            fprintf(stderr, "Error while calculating stopwords\n");
            exit(EXIT_FAILURE);
        }
    }
    if (c->show_progress) fprintf(stderr, "-- Writing databases\n");
    res = db_write(c);
    
    if (c->verbose)
        fprintf(stderr, "%lu tokens, %u files\n", c->t_ct, c->f_ni);
    
    if (res == 0 && c->compressed) res = gzip_tokens_file(c);
    
    free_context(c);
    free(c);
    free_nextline_buffer();
    return res;               /* for now */
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
    
    if (filter_enqueue_files(c) < 0) exit(EXIT_FAILURE);
    if (worker_init_all(c) < 0) exit(EXIT_FAILURE);
    
    fnlen = v_array_length(c->fnames);
    
    fdsr = (fd_set *) alloc(sizeof(fd_set), 's');
    
    if (gettimeofday(&tv, NULL) != 0) err(1, "gettimeofday");
    c->startsec = tv.tv_sec;
    tv.tv_sec = 0;
    tv.tv_usec = 10 * 1000;
    
    for (;;) {
        assert(tv.tv_sec == 0);
        assert(c->w_busy + c->w_avail == c->w_ct);
        if (c->w_avail > 0) action = worker_schedule(c);
        if (action < 0) {
            fprintf(stderr, "Error while scheduling worker process\n");
            exit(EXIT_FAILURE);
        }
        if (c->w_busy > 0) action = worker_check(c, fdsr) || action;
        
        if (action) {
            action = 0;
        } else if (c->f_ni == fnlen && c->w_busy == 0) {
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
