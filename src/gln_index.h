#ifndef GLN_INDEX_H
#define GLN_INDEX_H

#define BUF_SZ 4096

typedef struct worker {
        int s;                  /* socket */
        struct fname *fname;    /* current file name, if any */
        int off;                /* read offset */
        char *buf;              /* read buffer */
} worker;

typedef struct context {
        /* worker-related settings */
        int w_ct;               /* total worker count */
        int w_avail;            /* available workers */
        int w_busy;             /* busy workers */
        worker *ws;             /* workers */

        /* paths and files */
        char *wkdir;            /* + "/.gln/": where DB files are stored */
        char *root;             /* root path of indexed files */
        FILE *tlog;             /* token log */
        FILE *swlog;            /* stop word log */
        FILE *settings;         /* index settings */
        int fdb_fd;             /* filename DB descriptor */
        int tdb_fd;             /* token DB descriptor */
        FILE *find;             /* pipe to find */
        int max_tid;            /* max known token ID */
        int max_w_socket;       /* max worker socket file ID */
        re_group *reg;          /* REs for files to ignore */
        table *ft;              /* filename table */
        table *wt;              /* known words table */
        struct v_array *fnames; /* filename array */
        uint fni;               /* current filename index */

        /* other settings */
        int verbose;            /* verbosity */
        int use_stop_words;
        int case_sensitive;
        int show_progress;      /* show progress? */
        int index_dotfiles;
        int update;             /* update existing DBs? */
        int compressed;         /* compress token list file? */
        long startsec;          /* starting time */
        uint tick;              /* progress tick */
        uint tick_max;          /* this many ticks -> progress */
        ulong tct;              /* token count */
        ulong toct;             /* token occurrence count */
} context;

#endif
