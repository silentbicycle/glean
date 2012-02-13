#ifndef DB_H
#define DB_H

/* will be dynamically resized, starting value */
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

void init_zlib();
void free_zlib();
void init_db_files(context *c);
int write_db(context *c);

#endif
