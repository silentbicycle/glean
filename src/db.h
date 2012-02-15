#ifndef DB_H
#define DB_H

/* Starting value for compression buffers (resized on demand). */
#define DEF_BUF_SZ 128

typedef struct dbdata {
    int ffd;                /* filename db file descriptor */
    ulong fo;               /* filename db offset */
    int tfd;                /* token db file descriptor */
    ulong to;               /* token db offset */
    /* Values for data packing */
    char *buf;
    ulong o;                /* offset into current buffer*/
    ulong bufsz;
    char *dbuf;             /* deflation buffer */
    ulong dbufsz;
    ulong maxbufsz;         /* largest buffer needed for deflating */
} dbdata;

/* Init/free internal structures for zlib compression. */
void init_zlib();
void free_zlib();

/* Initialize new database files. */
void db_init_files(context *c);

/* Write the data set. */
int db_write(context *c);

/* Get the default DB base path. */
char *db_default_gln_dir();

#endif
