#ifndef GLN_INDEX_H
#define GLN_INDEX_H

#define BUF_SZ 4096

/* Information specific to a tokenizer worker process. */
typedef struct worker {
    int s;                  /* socket fd */
    struct fname *fname;    /* current file name, if any */
    int off;                /* read offset */
    char *buf;              /* read buffer */
} worker;

/* Overall indexing context. */
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
    int filter_fd;          /* fd for gln_filter coprocess */
    int filter_pid;         /* pid for same */
    int max_tid;            /* max known token ID */
    int max_w_socket;       /* max worker socket file ID */
    set *fn_set;            /* filename set */
    set *word_set;          /* known words set */
    struct v_array *fnames; /* filename array */
    uint f_ni;               /* current filename index */
    
    /* other settings */
    int verbose;            /* verbosity */
    int use_stop_words;     /* attempt to detect & ignore stop words? */
    int case_sensitive;     /* case-sensitive search? */
    int show_progress;      /* show progress? */
    int index_dotfiles;     /* should .dotfiles be indexed? */
    int update;             /* update existing DBs? */
    int compressed;         /* compress token list file? */
    long startsec;          /* starting time */
    uint tick;              /* progress tick */
    uint tick_max;          /* this many ticks -> progress */
    ulong t_ct;             /* token count */
    ulong t_occ_ct;         /* token occurrence count */
} context;

#endif
