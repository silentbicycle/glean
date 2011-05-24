#ifndef GLN_H
#define GLN_H

typedef struct ll_offset {
        ulong o;
        struct ll_offset *n;
} ll_offset;

enum grep_op {
        AND,        /* grep tok $files | grep tok2 */
        OR,         /* grep tok -e tok2 $files */
        NOT,        /* grep tok $files | grep -v tok2 */
        NEAR        /* NYI: grep -C$L tok $files | grep -C$L tok2 */
} grep_op;

/* File results to pass to grep pipeline */
typedef struct grep {
        enum grep_op op;
        char *pattern;
        struct v_array *tokens;   /* result filenames */
        struct h_array *thashes;  /* hashes for matching tokens from $GLN_DIR/tokens */
        struct h_array *results;  /* file hashes */
        struct grep *g;           /* another grep to pipe this to */
} grep;

typedef struct dbinfo {
        char *gln_dir;
        char *root;               /* root of indexed content */
        char *fdb;                /* mmap'd filename db */
        ll_offset *fdb_head;
        char *tdb;                /* mmap'd token db */
        ll_offset *tdb_head;
        char *tdfl_buf;           /* deflate buffer */
        char *fdfl_buf;           /* deflate buffer */
        uint buflen;

        struct grep *g;           /* query */
        struct h_array *results;  /* overall file hashes */
        struct v_array *fnames;   /* result filenames */
        /* settings, should be read from $GLN_DIR/settings */
        int verbose;
        int greponly;             /* 1=just print grep command line */
        int grepnames;            /* 0=no names, 1=show names, 2=names only */
        int subtoken;             /* 0=search tokens for ^%s$, 1=allow subtoken query */
        int compressed;           /* is the tokens file compressed? */
        int case_sensitive;
} dbinfo;


#endif
