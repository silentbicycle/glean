#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <regex.h>
#include <zlib.h>

#include "glean.h"
#include "hash.h"
#include "fhash.h"
#include "whash.h"
#include "gln_index.h"
#include "db.h"
#include "gln.h"
#include "dumphex.h"
#include "array.h"
#include "nextline.h"

#define HB HASH_BYTES

/* How many filenames to check at a time - if the filenames concat to
 * longer than ARG_MAX (typically, 256 kb), the buffer passed to grep
 * will cause a segfault.
 *
 * When used with multiple pipelined greps, a smaller batch size means
 * more processes used, but also faster results (due to buffering).
 */
#define BATCH_SIZE 100

static void usage() {
    puts("glean, by Scott Vokes\n"
        "usage: gln [-h] [-vgnNsDH] [-d db_path] QUERY\n"
        "where QUERY can include AND, OR, or NOT\n");
    exit(1);
}

static char *op_strs[] = { "", "AND", "OR", "NOT", /*"NEAR",*/ NULL };

static dbinfo *init_dbinfo() {
    dbinfo *db = alloc(sizeof(dbinfo), 'd');
    memset(db, 0, sizeof(dbinfo));
    db->gln_dir = default_gln_dir();
    db->grepnames = 1;
    db->compressed = 0;
    db->verbose = db->subtoken = db->tokens_only = 0;;
    return db;
}

static void bail(char *s) {
    fprintf(stderr, "%s", s);
    exit(1);
}

static const char *fndb_fn = "/.gln/fname.db";
static const char *tokdb_fn = "/.gln/token.db";

/* mmap the token and filename DBs */
static void open_dbs(dbinfo *db) {
    int ffd, tfd;
    size_t root_len, flen, tlen, fnlen = strlen(fndb_fn);
    struct stat sb;
    char *fn;
    int minlen = strlen("glnF " GLN_VERSION_STRING) + 4;
    root_len = strlen(db->gln_dir);
    fn = alloc(fnlen + root_len + 1, 'n');
    strncpy(fn, db->gln_dir, strlen(tokdb_fn) + root_len);
    
    strncat(fn, fndb_fn, fnlen);
    if (DEBUG) fprintf(stderr, "opening %s\n", fn);
    if ((stat(fn, &sb)) == -1) err(1, "%s", fn);
    flen = sb.st_size;
    if (flen < minlen) bail("Invalid fname.db.\n");
    if ((ffd = open(fn, O_RDONLY, 0)) == -1) err(1, "%s", fn);
    if ((db->fdb = mmap(NULL, flen, PROT_READ, MAP_PRIVATE, ffd, 0)) == MAP_FAILED)
        err(1, "%s", fn);
    
    strncpy(fn + root_len, tokdb_fn, fnlen);
    if (DEBUG) fprintf(stderr, "opening %s\n", fn);
    if ((stat(fn, &sb)) == -1) err(1, "%s", fn);
    tlen = sb.st_size;
    if (flen < minlen) bail("Invalid token.db.\n");
    if ((tfd = open(fn, O_RDONLY, 0)) == -1) err(1, "%s", fn);
    if ((db->tdb = mmap(NULL, tlen, PROT_READ, MAP_PRIVATE, tfd, 0)) == MAP_FAILED)
        err(1, "%s", fn);
    
    free(fn);
}

/* read $GLN_DIR/settings file */
static void read_settings(dbinfo *db) {
    FILE *settings;
    int path_len = strlen(db->gln_dir) + 2 + strlen("/.gln/settings");
    char *buf, *path = alloc(path_len, 'p');
    size_t len;
    
    snprintf(path, path_len, "%s/.gln/settings", db->gln_dir);
    settings = fopen(path, "r");
    if (settings == NULL) err(1, "%s", path);
    
#define OPT(x) (strncmp(buf, x, strlen(x)) == 0)
    while ((buf = nextline(settings, &len)) != NULL) {
        if (OPT("case_sensitive")) {
            db->case_sensitive = buf[len-2] == '1';
        } else if (OPT("compressed")) {
            db->compressed = buf[len-2] == '1';
        }
        /* other options go here later... */
    }
#undef OPT
    
    if (fclose(settings) != 0) err(1, "%s", path);
    
}

/* read 4 bytes into a u_int32_t, avoiding endianness issues */
static u_int32_t rd_int32(char *buf, ulong offset) {
    u_int32_t n = 0;
    int i;
    for (i=3; i>=0; i--) {
        char b = buf[offset + i];
        n = (n << 8) + (b & 0xff);
        if (DEBUG && 0) fprintf(stderr, "i: %d, b: %u -> n: %u\n",
            i, b & 0xff, n);
    }
    return n;
}

/* read 2 bytes into a u_int32_t, avoiding endianness issues */
static u_int16_t rd_int16(char *buf, ulong offset) {
    u_int16_t n = 0;
    int i;
    for (i=1; i>=0; i--) {
        char b = buf[offset + i];
        n = (n << 8) + (b & 0xff);
        if (DEBUG && 0) fprintf(stderr, "i: %d, b: %u -> n: %u\n",
            i, b & 0xff, n);
    }
    if (HB == 2) assert(n == (n % 0xffff));
    return n;
}

static u_int32_t rd_hash(char *buf, ulong offset) {
    if (HB == 4) return rd_int32(buf, offset);
    if (HB == 2) return rd_int16(buf, offset);
    assert(0);
}

static ll_offset *build_chain(char *p, uint offset) {
    ll_offset *cur, *prev = NULL;
    u_int32_t off;
    off = rd_int32(p, offset);
    while (off != 0) {
        cur = alloc(sizeof(ll_offset), 'o');
        cur->o = off;
        if (DEBUG) fprintf(stderr, "Adding offset %u (0x%04x)\n", off, off);
        cur->n = prev;  /* cons to front; newest results first */
        prev = cur;
        off = rd_int32(p, off);
        if (DEBUG) fprintf(stderr, "Next offset is %u (0x%04x)\n", off, off);
    }
    return prev;
}

static void check_db_headers(dbinfo *db) {
    uint offset;
    uint tbsz, fbsz;             /* token, filename buffer sizes */
    uint verlen = strlen(GLN_VERSION_STRING);
    if (strncmp(db->fdb, "glnF ", 5) != 0) bail("fname.db: bad header\n");
    if (strncmp(db->tdb, "glnT ", 5) != 0) bail("token.db: bad header\n");
    if (strncmp(db->fdb + 5, GLN_VERSION_STRING, verlen) != 0)
        bail("fname.db: bad db version, rebuild db\n");
    if (strncmp(db->tdb + 5, GLN_VERSION_STRING, verlen) != 0)
        bail("fname.db: bad db version, rebuild db\n");
    
    offset = 5 + strlen(GLN_VERSION_STRING) + 1;
    fbsz = rd_int32(db->fdb, offset);
    if (DEBUG) fprintf(stderr, "\nfdb, buf sz %d\n", fbsz);
    db->fdb_head = build_chain(db->fdb, offset + 4);
    
    tbsz = rd_int32(db->tdb, offset);
    if (DEBUG) fprintf(stderr, "\ntdb, buf sz %d\n", tbsz);
    db->tdb_head = build_chain(db->tdb, offset + 4);
    
    db->buflen = (fbsz > tbsz ? fbsz : tbsz) + 1;
    db->tdfl_buf = alloc(db->buflen, 'b');
    db->fdfl_buf = alloc(db->buflen, 'b');
}

static void free_grep(grep *g) {
    grep *ng;
    ng = g->g;
    if (g->thashes) h_array_free(g->thashes);
    /* g->results is aliased and freed by free_dbinfo below. */
    if (g->tokens) v_array_free(g->tokens, &free);
    free(g);
    if (ng) free_grep(ng);
}

static void free_dbinfo(dbinfo *db) {
    if (db->tdfl_buf) free(db->tdfl_buf);
    if (db->fdfl_buf) free(db->fdfl_buf);
    if (db->fnames) v_array_free(db->fnames, &free);
    if (db->results) h_array_free(db->results);
    if (db->g) free_grep(db->g);
    free(db);
}

static ulong uncompress_buffer(char *dfl_buf, ulong buflen, char *srcbuf, ulong srclen) {
    ulong destlen = buflen;
    int res = uncompress(dfl_buf, &destlen, srcbuf, srclen);
    assert(res == Z_OK);
    return destlen;
}


/*************
 * Filenames *
 *************/

static void dump_fname_bucket(dbinfo *db, ulong o) {
    int i, xo = DB_X_CT;
    ulong len = rd_int32(db->fdb, o + xo); /* compressed byte count */
    ulong zo = o + 4 + xo;
    ulong off, hash, noff;
    char *dfl_buf = db->fdfl_buf;
    
    int fnames = 0, fname_bytes = 0;
    
    if (xo > 0) for (i=0; i<xo; i++) assert(db->fdb[o + i] == 'X');
    if (DEBUG) fprintf(stderr, "Compressed:\t%lu bytes\n", len);
    len = uncompress_buffer(dfl_buf, db->buflen, db->fdb + zo, len);
    if (DEBUG) fprintf(stderr, "Deflated:\t%lu bytes\n", len);
    
    if (DEBUG) dumphex(stderr, dfl_buf, len);
    off = 0;
    do {
        noff = rd_int32(dfl_buf, off);
        hash = rd_hash(dfl_buf, off + 4);
        len = strlen(dfl_buf + off + 4 + HB);
        if (db->verbose > 0) printf("hash: 0x%04lx, len: %ld\t %s\n",
            hash, len, dfl_buf + off + 4 + HB);
        fnames++; fname_bytes += len;
        off = noff;
    } while (off != 0);
    printf("f: %d filenames, %d bytes\n", fnames, fname_bytes);
}


/**********
 * Tokens *
 **********/

static void dump_token_bucket(dbinfo *db, ulong o) {
    int i, xo = DB_X_CT;
    ulong len = rd_int32(db->tdb, o + xo); /* compressed byte count */
    ulong zo = o + 4 + xo;
    ulong off, hash, fhash, noff;
    char *dfl_buf = db->tdfl_buf;
    int vb = db->verbose > 0;
    int tokens=0, token_bytes=0, token_hash_bytes=0;
    
    if (xo > 0) for (i=0; i<xo; i++) assert(db->tdb[o + i] == 'X');
    if (DEBUG) fprintf(stderr, "Compressed:\t%lu bytes\n", len);
    len = uncompress_buffer(dfl_buf, db->buflen, db->tdb + zo, len);
    if (DEBUG) fprintf(stderr, "Deflated:\t%lu bytes\n", len);
    
    if (DEBUG) dumphex(stderr, dfl_buf, len);
    off = 0;
    do {
        noff = rd_int32(dfl_buf, off);
        hash = rd_hash(dfl_buf, off + 4);
        len = rd_int16(dfl_buf, off + 4 + HB);
        tokens++; token_bytes += len;
        if (vb) printf("token: 0x%04lx, files:", hash);
        for (i=0; i<len; i++) {
            fhash = rd_hash(dfl_buf, off + 6 + HB + i*HB);
            if (vb) printf(" %04lx", fhash);
            token_hash_bytes += HB;
        }
        if (vb) puts("");
        off = noff;
    } while (off != 0);
    printf("b: %d tokens, %d token bytes, %d token hash bytes\n",
        tokens, token_bytes, token_hash_bytes);
}


/********
 * Main *
 ********/

/* Dump the filename and token databases to stdout, for debugging. */
static void dump_db(dbinfo *dbi, char *db, ll_offset *db_head,
    void (*read_bucket)(dbinfo *db, ulong)) {
    int t_ct = 0;                      /* table count */
    ulong o, buckets, i, head; /* table head */
    ll_offset *cur;
    printf("\n-- Dump db -- ");
    for (cur=db_head; cur != NULL; cur=cur->n) {
        o = cur->o;
        head = o + 4;
        buckets = rd_int32(db, head)/4;
        printf("Bucket count: %lu\n", buckets);
        
        for (i=0; i<buckets; i++) {
            o = rd_int32(db, head + 4 + (i*4));
            printf("\nBucket %ld, offset: %lu (0x%04lx), head %lu (0x%04lx)\n",
                i, o, o, head, head);
            read_bucket(dbi, o);
        }
        t_ct++;
    }
}

/* Just print word hashes (for testing DBs). */
static int hash_loop(char *buf) {
    uint hash;
    for (;;) {
        if (fgets(buf, MAX_WORD_SZ, stdin) != NULL) {
            buf[strcspn(buf, "\n")] = '\0';
            hash = hash_word(buf);
            printf("%s %u 0x%04x\n", buf, hash, hash);
        } else break;
    }
    return 0;
}

static void append_matches_in_bucket(dbinfo *db, hash_t tokhash,
    uint b_offset, h_array *fs) {
    int i, xo = DB_X_CT;
    ulong len = rd_int32(db->tdb, b_offset + xo); /* compressed byte count */
    ulong zo = b_offset + 4 + xo;
    ulong off, hash, fhash, noff;
    char *dfl_buf = db->tdfl_buf;
    if (xo > 0) for (i=0; i<xo; i++) assert(db->tdb[b_offset + i] == 'X');
    len = uncompress_buffer(dfl_buf, db->buflen, db->tdb + zo, len);
    
    off = 0;
    do {
        noff = rd_int32(dfl_buf, off);
        hash = rd_hash(dfl_buf, off + 4);
        len = rd_int16(dfl_buf, off + 4 + HB);
        if (DEBUG) fprintf(stderr, "noff: %04lx\thash: %04lx\tlen: %lu\ttokhash: %04x\n",
            noff, hash, len, tokhash);
        if (hash == tokhash) {
            for (i=0; i<len; i++) {
                fhash = rd_hash(dfl_buf, off + 6 + HB + i*HB);
                h_array_append(fs, fhash);
            }
        }
        off = noff;
    } while (off != 0);
}

static void append_token_files(dbinfo *db, hash_t hash, h_array *fs) {
    ll_offset *cur;
    uint buckets, b, bo; /* bucket number, bucket offset */
    
    for (cur=db->tdb_head; cur != NULL; cur=cur->n) {
        buckets = rd_int32(db->tdb, cur->o + 4)/4;
        b = hash % buckets;
        bo = rd_int32(db->tdb, cur->o + 8 + b*4);
        if (DEBUG) fprintf(stderr, "buckets: %d; hash: 0x%04x; b:%d\n",
            buckets, hash, b);
        append_matches_in_bucket(db, hash, bo, fs);
        /* get_fns(db, hash, bo); */
    }
}


/******************
 * Query pipeline *
 ******************/

static grep *init_grep(char *pattern, enum grep_op op, grep *parent) {
    grep *g = alloc(sizeof(grep), 'g');
    g->op = op;
    g->pattern = pattern;
    g->tokens = v_array_init(4);
    g->thashes = h_array_init(4);
    g->results = h_array_init(4);
    if (parent) parent->g = g;
    g->g = NULL;
    return g;
}

static void build_query(dbinfo *db, int *argc, char **argv[]) {
    int i;
    char *pat;
    grep *g = NULL, *lastg = NULL;
    enum grep_op op = AND;
    
    for (i=0; i<*argc; i++) {
        pat = (*argv)[i];
        if (strcmp(pat, "AND") == 0) { op = AND; }
        else if (strcmp(pat, "OR") == 0) { op = OR; }
        else if (strcmp(pat, "NOT") == 0) { op = NOT; }
        else if (strcmp(pat, "NEAR") == 0) { op = NEAR; }
        else {
            if (op == AND || lastg != NULL) {
                g = init_grep(pat, op, lastg);
                if (lastg == NULL) db->g = g;
            }
            lastg = g;
            op = AND;
        }
    }
}

static void format_cmd(dbinfo *db, char *cmd, char *pat, char *tokpath) {
    char *gz = (db->compressed ? "z" : "");
    snprintf(cmd, PATH_MAX + 1,
        (db->subtoken ? "%sgrep \"%s\" %s %s |sort|uniq"
            : "%sgrep %s \"^%s$\" %s |sort|uniq"),
        gz, db->case_sensitive ? "" : "-i", pat, tokpath);
}

static void get_matching_tokens(dbinfo *db) {
    grep *g;
    char *pat, *buf, *tok;
    size_t len;
    char tokpath[PATH_MAX + 1], cmd[PATH_MAX + 1];
    FILE *pipe;
    hash_t hash;
    uint ct;
    
    snprintf(tokpath, PATH_MAX + 1, "%s/.gln/tokens%s",
        db->gln_dir, db->compressed ? ".gz" : "");
    
    for (g = db->g; g != NULL; g = g->g) {
        pat = g->pattern;
        format_cmd(db, cmd, pat, tokpath);
        if ((pipe = popen(cmd, "r")) == NULL) err(1, "popen fail");
        while ((buf = nextline(pipe, &len))) {
            if (buf[len - 1] == '\n') buf[len - 1] = '\0';
            tok = alloc(len, 't');
            strncpy(tok, buf, len);
            v_array_append(g->tokens, tok);
            hash = hash_word(buf);
            h_array_append(g->thashes, hash);
            if (db->tokens_only) printf("%s\n", tok);
        }
        ct = h_array_length(g->thashes);
        if (ct > TOO_MANY_MATCHES)
            fprintf(stderr, "Warning: Pattern '%s' matched %u tokens\n", pat, ct);
        h_array_sort(g->thashes);
    }
}

static void get_matching_file_hashes(dbinfo *db) {
    grep *g;
    hash_t hash;
    uint i;
    for (g = db->g; g != NULL; g = g->g) {
        for (i = 0; i < h_array_length(g->thashes); i++) {
            hash = h_array_get(g->thashes, i);
            append_token_files(db, hash, g->results);
        }
        h_array_sort(g->results);
        h_array_uniq(g->results);
    }
}

static void dump_grep(grep *head) {
    int i=0;
    grep *g;
    for (g = head; g != NULL; g=g->g) {
        printf("%-4s %s:", op_strs[g->op], g->pattern);
        for (i=0; i < h_array_length(g->results); i++) {
            printf(" %04x", h_array_get(g->results, i));
        }
        puts("");
    }
}

static void filter_results(dbinfo *db) {
    h_array *res = NULL, *nres = NULL;
    grep *g;
    assert(db->g);
    for (g = db->g; g != NULL; g=g->g) {
        if (res) {
            if (g->op == AND || g->op == NEAR) {
                nres = h_array_intersection(g->results, res);
            } else if (g->op == OR) {
                nres = h_array_union(g->results, res);
            } else if (g->op == NOT) {
                nres = h_array_complement(res, g->results);
            } else {
                err(1, "match fail");
            }
            assert(nres);
            if (nres != res) { free(res); if (res) res = nres; }
        } else if (g->results) {
            assert(g->results);
            res = g->results;
        }
    }
    assert(res);
    db->results = res;
}

static void append_matching_fnames(dbinfo *db, uint fhash, uint o) {
    int i, xo = DB_X_CT;
    ulong len = rd_int32(db->fdb, o + xo); /* compressed byte count */
    ulong zo = o + 4 + xo;
    ulong off, hash, noff;
    char *buf = db->fdfl_buf, *fn = NULL;
    
    if (xo > 0) for (i=0; i<xo; i++) assert(db->fdb[o + i] == 'X');
    len = uncompress_buffer(buf, db->buflen, db->fdb + zo, len);
    off = 0;
    do {
        noff = rd_int32(buf, off);
        hash = rd_hash(buf, off + 4);
        len = strlen(buf + off + 4 + HB);
        if (hash == fhash) {
            fn = alloc(len + 1, 'f');
            strncpy(fn, buf + off + 4 + HB, len + 1);
            v_array_append(db->fnames, fn);
        }
        off = noff;
    } while (off != 0);
}

static void append_hash_files(dbinfo *db, hash_t fhash) {
    ll_offset *cur;
    uint buckets, o, b, bo, head; /* bucket number, bucket offset */
    
    for (cur=db->fdb_head; cur != NULL; cur=cur->n) {
        o = cur->o;
        head = o + 4;
        buckets = rd_int32(db->fdb, head)/4;
        b = fhash % buckets;
        bo = rd_int32(db->fdb, cur->o + 8 + b*4);
        append_matching_fnames(db, fhash, bo);
    }
}

static char *get_timestamp_fname(dbinfo *db) {
    uint len;
    char *tsfile = alloc(MAXPATHLEN, 'p');
    len = strlen(db->gln_dir);
    strncpy(tsfile, db->gln_dir, MAXPATHLEN);
    strncpy(tsfile + len, "/.gln/timestamp ", MAXPATHLEN - len);
    return tsfile;
}

static void get_matching_filenames(dbinfo *db) {
    uint i;
    db->fnames = v_array_init(2);
    for (i=0; i<h_array_length(db->results); i++)
        append_hash_files(db, h_array_get(db->results, i));
}

/* This should already be defined... */
#ifndef ARG_MAX
#define ARG_MAX 256 * 1024
#endif

static const char *grepnames_opt[] = {"-h ", "", "-l "};

static void run_pipeline(dbinfo *db, int file_offset, int file_ct) {
    grep *g;
    uint i, gnum = 0;           /* grep pattern number */
    char *tok, *fn;
    char fnbuf[ARG_MAX], cmd[ARG_MAX];
    uint len, fo = 0, fp = 0;   /* file buf offset; files printed? */
    uint co = 0, extra;         /* cmd buf offset and add'l padding */
    FILE *pipe;
    char *buf, *gnstr, *cwd;
    size_t plen, cwdlen;
    
    /* Append $GLN_DIR/timestamp, an empty file, so if the index only finds
     * one file, its name will still be printed.  */
    char *tsfile = get_timestamp_fname(db);
    len = strlen(tsfile);
    strncpy(fnbuf, tsfile, len);
    fo += len;
    free(tsfile);
    
    /* concat all filenames in fnbuf */
    for (i=file_offset; i<file_offset + file_ct; i++) {
        fn = (char *)v_array_get(db->fnames, i);
        len = strlen(fn);
        if (fo + 2 + len > ARG_MAX) {
            fprintf(stderr, "Too many filenames for grep pipeline.\n");
            exit(1);
        }
        fnbuf[fo++] = '"';
        strncpy(fnbuf + fo, fn, len);
        fo += len;
        fnbuf[fo++] = '"';
        fnbuf[fo] = ' '; fnbuf[++fo] = '\0';
    }
    
    for (g = db->g; g != NULL; g=g->g) {
        extra = 0;
        /* should it get a new |grep? */
        if (g->op == AND || g->op == NOT) {
            if (gnum > 0 && fp == 0) { /* add filenames here? */
                strncpy(cmd + co, fnbuf, fo);
                co += fo; fp = 1;
            }
            assert(db->grepnames >= 0 && db->grepnames <= 2);
            gnstr = (gnum > 0 ? "" : grepnames_opt[db->grepnames]);
            
            snprintf(cmd + co, ARG_MAX, "%sgrep %s%s",
                gnum > 0 ? "|" : "",
                gnstr, (db->case_sensitive ? "" : "-i "));
            if (gnum > 0) extra++;
            extra += strlen(gnstr);
            if (!db->case_sensitive) extra += 3;
            co += 5 + extra;
        }
        
        /* NOT -> -v */
        if (g->op == NOT) { strncpy(cmd + co, "-v ", 3); co += 3; }
        
        /* -e tok -e tok2 ... */
        for (i=0; i<v_array_length(g->tokens); i++) {
            tok = (char *)v_array_get(g->tokens, i);
            snprintf(cmd + co, ARG_MAX, "-e %s ", tok);
            co += 4 + strlen(tok);;
        }
        gnum++;                
    }
    
    /* put filenames at end if not already used */
    if (fp == 0) {
        strncpy(cmd + co, fnbuf, fo);
        cmd[co + fo] = '\0';
    }
    
    if (db->greponly) { printf("%s\n", cmd); return; }
    
    if (db->verbose > 1) printf("-- Cmd is:\n%s\n", cmd);
    
    if ((cwd = getcwd(NULL, MAXPATHLEN)) == NULL) err(1, "getcwd");
    cwdlen = strlen(cwd);
    
    if ((pipe = popen(cmd, "r")) == NULL) err(1, "popen fail");
    while ((buf = nextline(pipe, &plen))) {
        if (buf[plen - 1] == '\n') buf[plen - 1] = '\0';
        
        if (db->grepnames == 0) {
            i = 0;  /* no path/name printed, so leave as-is */
        } else {
            /* if file is in subdir of current path, make relative path */
            for (i=0; i<cwdlen; i++) {
                if (0) printf("bi: %d (%c), ci: %d\n",
                    buf[i], buf[i], cwd[i]);
                if (buf[i] == '\0' || buf[i] != cwd[i]) break;
            }
            if (i <= cwdlen - 1) i=0;
            else if (buf[i] == '/') i++;
        }
        printf("%s\n", buf + i);
    }
    free(cwd);
    if (pclose(pipe) == -1) err(1, "pclose fail");
}

static int fn_cmp(const void *a, const void *b) {
    char *fna = ((fname *)a)->name;
    char *fnb = ((fname *)b)->name;
    return strcmp(fna, fnb);
}

static void lookup_query(dbinfo *db) {
    int i, fnct, rem;
    char *fn;
    
    get_matching_tokens(db);
    if (db->tokens_only) {
        
        return;
    }
    get_matching_file_hashes(db);
    if (db->verbose > 1) dump_grep(db->g);
    
    filter_results(db);
    if (db->verbose) {
        printf("\nfile hashes --");
        for (i=0; i<h_array_length(db->results); i++) {
            printf(" %04x", h_array_get(db->results, i));
        }
        puts("");
    }
    get_matching_filenames(db);
    v_array_sort(db->fnames, fn_cmp);
    
    if (db->verbose) {
        for (i=0; i<v_array_length(db->fnames); i++) {
            fn = (char *)v_array_get(db->fnames, i);
            printf("    %s\n", fn);
        }
    }
    
    /* If nothing is found (besides $GLN_DIR/timestamp), bail */
    if (v_array_length(db->fnames) == 0) {
        fprintf(stderr, "No matching files found.\n");
        exit(0);
    }
    
    fnct = v_array_length(db->fnames);
    for (i=0; i + BATCH_SIZE < fnct; i+= BATCH_SIZE) run_pipeline(db, i, BATCH_SIZE);
    if ((rem = fnct % BATCH_SIZE) > 0) run_pipeline(db, i, rem); /* do remaining */
}


/********
 * Main *
 ********/

static char handle_args(dbinfo *db, int *argc, char **argv[]) {
    int fl;
    char mode = 'g';
    while ((fl = getopt(*argc, *argv, "hDHvd:nNgst")) != -1) {
        switch (fl) {
        case 'h':       /* help */
            usage();
            break;
        case 'D':       /* dump db */
            mode = 'D';
            break;
        case 'H':       /* hash input tokens */
            mode = 'H';
            break;
        case 'v':
            db->verbose++;
            break;
        case 'd':       /* set db directory */
            db->gln_dir = (strcmp(optarg, ".") == 0 ? 
                getcwd(NULL, MAXPATHLEN) : optarg);
            break;
        case 'n':       /* names only */
            db->grepnames = 2;
            break;
        case 'N':       /* no names */
            db->grepnames = 0;
            break;
        case 's':       /* "subtoken": don't wrap token pattern in ^$ */
            db->subtoken = 1;
            break;
        case 't':       /* "token", print matching tokens and exit */
            db->tokens_only = 1;
        case 'g':       /* don't actually grep, just print cmd */
            db->greponly = 1;
            break;
        default:
            usage();
            /* NOTREACHED */
        }
    }
    *argc -= optind;
    *argv += optind;
    
    if (*argc == 0 && mode == 'g') usage();
    
    build_query(db, argc, argv);
    return mode;
}

int main(int argc, char *argv[]) {
    char buf[MAX_WORD_SZ];
    dbinfo *db;
    char mode = 'g';
    
    db = init_dbinfo();
    mode = handle_args(db, &argc, &argv);
    
    if (mode == 'H') {
        free_dbinfo(db);
        return hash_loop(buf);
    }
    
    init_zlib();
    open_dbs(db);
    read_settings(db);
    check_db_headers(db);
    
    if (mode == 'D') {        /* dump */
        dump_db(db, db->fdb, db->fdb_head, dump_fname_bucket);
        dump_db(db, db->tdb, db->tdb_head, dump_token_bucket);
    } else if (mode == 'g'){  /* glean db lookup, default */
        lookup_query(db);
    }
    free_dbinfo(db);
    free_nextline_buffer();
    free_zlib();
    return 0;
}
