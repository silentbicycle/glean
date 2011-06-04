#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>
#include <poll.h>
#include <regex.h>

#include "glean.h"
#include "array.h"
#include "nextline.h"

/* filter program: gln_index passes results from "find ." to it line by line,
 * it responds with "ignore" or "index". Run as a coprocess. */

#define TIMEOUT 60              /* just die after 1 minute idle */

typedef struct re_group {
        int debug;
        struct v_array *res;    /* regex_t array */
        struct v_array *ans;    /* response array */
} re_group;

/* Default REs for filenames to always ignore. */
static char *default_ignore_REs[] = {
        /* version control systems */
        "\\.git/", "\\.hg/", "CVS/",
        /* emacs backup files */
        "~$",
        /* Various extensions for file types that may begin w/ text headers. */
        "\\.mp3$", "\\.pdf$", "\\.jpg$", "\\.ogg$", "\\.ppt$",
        "\\.zip$", "\\.chm$",
        /* others? */
        NULL,
};

static re_group *init_re_group() {
        re_group *g = alloc(sizeof(re_group), 'r');
        g->res = v_array_init(4);
        g->ans = v_array_init(4);
        g->debug = 0;
        return g;
}

static void free_re(void *v) { regfree((regex_t *) v); }

static void free_re_group(re_group *g) {
        v_array_free(g->res, free_re);
        v_array_free(g->ans, free);
        free(g);
}

static char *get_pattern_file(int argc, char **argv) {
        char *e = getenv("GLN_FILTER_FILE");
        if (e != NULL) {
                int len = strlen(e);
                char *path, *home;
                if (len > 1 && e[0] == '~' && e[1] == '/') {
                        home = getenv("HOME");
                        assert(home);
                        len = strlen(e) + strlen(home) + 1;
                        path = alloc(len, 'p'); /* leaked. not a big deal. */
                        snprintf(path, len, "%s/%s", home, e + 2);
                        return path;
                }
                return e;
        }
        if (argc > 1) return argv[1];
        return NULL;
}

static void add_pattern(re_group *g, char *pat, char *action) {
        regex_t *re = alloc(sizeof(regex_t), 'r');
        char *caction;   /* copy of action string */
        int ok, len;
        assert(pat != NULL); assert(g != NULL); assert(action != NULL);
        if ((ok = regcomp(re, pat, REG_EXTENDED | REG_ICASE) != 0)) {
                fprintf(stderr, "Bad regex: %s\n", pat);
                err(1, "regcomp");
        }
        len = strlen(action) + 1;
        caction = alloc(len, 'a');
        strncpy(caction, action, len);
        v_array_append(g->res, re);
        v_array_append(g->ans, caction);
        if (g->debug) printf("Added re #%d: %s\n", v_array_length(g->res), pat);
}

static void extract_pattern(re_group *g, char *buf, size_t len) {
        char *re, *action;
        int i;
        if (len == 0 || buf[0] == '#') return; /* skip blank line / comment */
        if (buf[0] == '"') {
                for (i=1; i < len && buf[i] != '"' && buf[i] != '\0'; i++) ;
                if (buf[i] == '\0') {
                        fprintf(stderr, "Bad pattern line: %s\n", buf);
                        exit(1);
                }
                re = buf + 1;
        } else {
                for (i=0; i < len && buf[i] != '\0' && buf[i] != ' '; i++) ;
                re = buf;
        }
                
        buf[i] = '\0';
        action = (i == len ? "ignore" : buf + i +1);
        assert(re); assert(action);
        add_pattern(g, re, action);
}

static void read_patterns(re_group *g, char *fname) {
        char **pat;
        if (fname == NULL) {
                for (pat = default_ignore_REs; *pat != NULL; pat++)
                        add_pattern(g, *pat, "ignore");
        } else {
                FILE *f = fopen(fname, "r");
                char *buf;
                size_t len;
                if (f == NULL) err(1, fname);
                while ((buf = nextline(f, &len))) {
                        if (buf[len-1] == '\n') { buf[len-1] = '\0'; len--; }
                        extract_pattern(g, buf, len);
                }
                if (fclose(f) != 0) err(1, fname);
        }
}

static void match_buf(re_group *g, char *buf) {
        int i, ok, len=v_array_length(g->res);
        regmatch_t pmatch;
        for (i=0; i<len; i++) {
                ok = regexec(v_array_get(g->res, i), buf, 1, &pmatch, 0);
                if (ok == 0) {
                        if (g->debug)
                                fprintf(stderr, "Rule %d matches: %s\n", i, buf);
                        printf("%s\n", (char *)v_array_get(g->ans, i));
                        return;
                } else {
                }
        }
        if (g->debug)
                fprintf(stderr, "Failed to match: %s\n", buf);
        printf("index\n");      /* default */
        
}

static void match_loop(re_group *g) {
        size_t len;
        char *buf;
        int res;
        struct pollfd fds[1];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;

        for (;;) {
                res = poll(fds, 1, TIMEOUT * 1000);
                if (res == -1) err(1, "poll");
                else if (res == 0) exit(1); /* timeout, gln_index probably died */
                else assert(fds[0].revents & POLLIN);

                buf = nextline(stdin, &len);
                if (buf == NULL) break;
                if (len > 0) buf[len-1] = '\0';
                match_buf(g, buf);
                fflush(NULL);
        }
}

int main(int argc, char **argv) {
        re_group *g = init_re_group();
        char *fname;
        if (getenv("GLN_FILTER_DEBUG") != NULL) g->debug = 1;
        else if (argc > 1 && strcmp(argv[1], "-d") == 0) {
                g->debug = 1;
                argc--;
                argv++;
        }

        fname = get_pattern_file(argc, argv);

        read_patterns(g, fname);
        match_loop(g);
        free_re_group(g);
        return 0;
}
