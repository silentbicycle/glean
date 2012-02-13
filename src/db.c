#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <zlib.h>
#include <sys/types.h>          /* for u_int32_t etc. */
#include <ctype.h>

#include "glean.h"
#include "hash.h"
#include "whash.h"
#include "fhash.h"
#include "hash.h"
#include "array.h"
#include "gln_index.h"
#include "db.h"
#include "dumphex.h"

/*
 * $WRKDIR/.gln/
 *    tokens.db
 *    files.db
 *    timestamp   (0-byte file, used for find, when updating)
 *    config      (options used for this db)
 *    ignore      (ignore settings)
 *
 *    when building, tokens.db.new, files.db.new, timestamp.new
 *  */

typedef ulong (pack_fun)(context *c, dbdata *d, tlink *t);

#define HB HASH_BYTES

static dbdata *init_dbdata(int fdb_fd, int tdb_fd) {
    dbdata *db = alloc(sizeof(dbdata), 'd');
    db->ffd = fdb_fd;
    db->fo = 0;
    db->tfd = tdb_fd;
    db->to = 0;
    db->buf = alloc(DEF_BUF_SZ, 'b');
    db->dbuf = alloc(DEF_BUF_SZ, 'b');
    db->bufsz = DEF_BUF_SZ;
    db->dbufsz = DEF_BUF_SZ;
    db->maxbufsz = 0;
    db->o = 0;
    return db;
}

static z_stream strm;

void init_zlib() {
    int res;
    /* custom allocators could be used here */
    strm.zalloc = Z_NULL; strm.zfree = Z_NULL; strm.opaque = Z_NULL;
    res = deflateInit(&strm, DEF_ZLIB_COMPRESS);
    assert(res == Z_OK);
}

void free_zlib() {
    int res = deflateEnd(&strm);
    assert(res == Z_OK);
}

static void buf_int32(char *buf, u_int32_t n, ulong offset) {
    int i;
    if (DEBUG && 0) printf("%d -> ", n);
    for (i=0; i<4; i++) {
        char b = (n & 0xff);
        if (buf) buf[offset + i] = b;
        n >>= 8;
        if (DEBUG && 0) printf("%u ", b & 0xff);
    }
    if (DEBUG && 0) printf("\n");
}

static void buf_int16(char *buf, u_int16_t n, ulong offset) {
    int i;
    if (DEBUG && 0) printf("%d -> ", n);
    for (i=0; i<2; i++) {
        char b = (n & 0xff);
        if (buf) buf[offset + i] = b;
        n >>= 8;
        if (DEBUG && 0) printf("%u ", b & 0xff);
    }
    if (DEBUG && 0) printf("\n");
}

static void buf_hash(char *buf, u_int32_t n, ulong offset) {
    if (HB == 4) { buf_int32(buf, n, offset); return; }
    if (HB == 2) { buf_int16(buf, n, offset); return; }
    assert(0);
}

static void write_int32(int fd, u_int32_t n) {
    char buf[4];
    buf_int32(buf, n, 0);
    write(fd, buf, 4);
}

/* Keep stats on compression ratios? */
#if PROFILE_COMPRESSION
static ulong db_bytes_in = 0;
static ulong db_bytes_out = 0;
#endif

/* Resize output buffer by +sz */
static void grow_buf(dbdata *db, ulong sz) {
    char *nbuf;
    ulong nsz = db->bufsz + sz;
    if (nsz > MAX_MEMORY) err(1, "buffer too large");
    if (DB_DEBUG) fprintf(stderr, "-- Resizing buf from %lu to %lu\n", db->bufsz, nsz);
    assert(db->buf);
    nbuf = realloc(db->buf, nsz);
    if (nbuf == NULL) err(1, "realloc fail");
    db->buf = nbuf;
    db->bufsz += sz; /* out of memory before overflow */
}

/* Resize compression buffer by +sz */
static void grow_dbuf(dbdata *db, ulong sz) {
    char *ndbuf;
    ulong nsz = db->dbufsz + sz;
    if (nsz > MAX_MEMORY) err(1, "buffer too large");
    if (DB_DEBUG) fprintf(stderr, "-- Resizing dbuf from %lu to %lu\n", db->dbufsz, nsz);
    
    assert(db->dbuf);
    ndbuf = realloc(db->dbuf, nsz);
    if (ndbuf == NULL) {
        fprintf(stderr, "Cannot realloc %lu bytes\n", sz);
        err(1, "realloc fail");
    }
    db->dbuf = ndbuf;
    db->dbufsz += sz; /* out of memory before overflow */
}

/* Compress current buffer into destination buffer, resizing as necessary. */
static ulong compress_buffer(dbdata *db, int pad) {
    ulong destlen = db->dbufsz;
    int res, srclen = db->o;
    if (SKIP_COMPRESS) { db->dbuf = db->buf; return db->o; }
    
    /* FIXME: While compress *should* just return Z_BUF_ERROR when the buffer is
     * not large enough, it seems to cause a crash on Linux.
     * Valgrind points to an off-by-one error occurring in compress. */
    while (db->dbufsz < srclen) grow_dbuf(db, db->dbufsz);
    
    if (DB_DEBUG) fprintf(stderr, "compressing: %d in dbuf of sz %lu\n", srclen, destlen);
    res = compress(db->dbuf + pad, &destlen, db->buf, srclen);
    if (DB_DEBUG) fprintf(stderr, "res:%d, destlen %lu\n", res, destlen);
    while (res == Z_BUF_ERROR) {
        if (DB_DEBUG) fprintf(stderr, "z_buf_error, resizing\n");
        grow_dbuf(db, db->dbufsz); /* double size */
        destlen = db->dbufsz;
        res = compress(db->dbuf + pad, &destlen, db->buf, srclen);
        if (DB_DEBUG) fprintf(stderr, " (r) res:%d, destlen %lu\n", res, destlen);
    }
    
    if (DB_DEBUG) printf("In: %ld\tOut: %ld\tRes: %d\n", db->o, destlen, res);
    if (res != Z_OK) {
        fprintf(stderr, "ZLib error: %d\n", res);
        fprintf(stderr, "in buf: %d, out buf: %lu, destlen: %lu\n",
            srclen, db->dbufsz, destlen);
        exit(1);
    }
    assert(res == Z_OK);
#if PROFILE_COMPRESSION
    db_bytes_in += db->o;
    db_bytes_out += destlen;
#endif
    db->maxbufsz = srclen > db->maxbufsz ? srclen : db->maxbufsz;
    return destlen;
}


/******************/
/* Database files */
/******************/

void init_db_files(context *c) {
    /*printf("New db files\n");*/
}


/*************
 * Filenames *
 *************/

static char *gln_file_header = "glnF " GLN_VERSION_STRING " ";

static ulong pack_fname_bucket(context *c, dbdata* db, tlink *tl) {
    fname *fn;
    tlink *cur;
    char *name;
    uint fhash;
    ulong co = 0, lo = 0, len;  /* current + last offsets */
    int i, link=0, xo=DB_X_CT;
    
    /* filename bucket buffer format:
     * [byte length for compressed bucket/4]
     * This portion is deflated:
     *   [next fname offset (relative), or NULL/4]
     *    Repeated: [hash/4] [fname and \0]
     */
    assert(db->o == 0);
    for (cur = tl; cur != NULL; cur = cur->next) {
        fn = (fname *)cur->v;
        name = fn->name;
        len = strlen(name) + 1;
        if (DB_DEBUG && 0)
            fprintf(stderr, "Packing link %d starting at %lu\n", link++, db->o);
        
        while (db->bufsz <= db->o + (4 + HB + len + 4 + xo))
            grow_buf(db, db->bufsz);
        
        co = db->o;
        buf_int32(db->buf, co, lo);   /* prev->this */
        buf_int32(db->buf, 0, co);    /* set to 0 (for now) */
        db->o += 4;
        
        fhash = hash_word(name);
        buf_hash(db->buf, fhash, db->o);
        db->o += HB;
        if (DB_DEBUG) fprintf(stderr, "  %lu (+%ld) -> %04x : %s\n",
            len, db->o, fhash, name);
        
        if (DB_DEBUG) fprintf(stderr, "bufsz: %lu, db->o: %lu, len: %lu\n", db->bufsz, db->o, len);
        while (db->bufsz <= db->o + len) { grow_buf(db, db->bufsz); }
        
        if (DB_DEBUG) fprintf(stderr, "old o: %lu, plus len: %lu\n", co, len);
        memcpy(db->buf + db->o, name, len);
        db->o += len + 1;
        
        lo = co;
    }
    len = compress_buffer(db, xo + 4); /* +4: shift to include data length */
    if (DB_DEBUG) fprintf(stderr, "Adding compressed buffer length %lu (0x%04lx)\n", len, len);
    for (i=0; i<xo; i++) db->dbuf[i] = 'X';
    buf_int32(db->dbuf, len, xo); /* compressed byte count at head */
    return len + 4 + xo;
}


/**********
 * Tokens *
 **********/

static char *gln_token_header = "glnT " GLN_VERSION_STRING " ";

static ulong pack_token_bucket(context *c, dbdata* db, tlink *tl) {
    word *w;
    tlink *cur;
    ulong co = 0, lo = 0, len;  /* current + last word offsets */
    ulong lho = 0, hashct;           /* last hash offset */
    int i, link = 0, xo=DB_X_CT;
    uint hash;
    h_array *a;
    
#define DEBUG_WD 0
    
    /* token bucket buffer format:
     * [byte length for compressed bucket/4]
     * This portion is deflated:
     *   [next word offset (relative), or NULL/4]
     *   [word hash/HB] [file hash count/2] [file hashes/HB*N]
     */
    assert(db->o == 0);
    for (cur = tl; cur != NULL; cur = cur->next) {
        if (DB_DEBUG && 0)
            fprintf(stderr, "Packing link %d starting at %lu\n", link++, db->o);
        
        w = (word *)cur->v;
        fprintf(c->tlog, "%s\n", w->name); /* append words to log */
        hash = hash_word(w->name);
        a = w->a;
        assert(a);
        if (DEBUG_WD) fprintf(stderr, "Word is %s (%04x): %u occs (%d), ",
            w->name, hash, w->i, w->stop);
        
        while (db->bufsz <= db->o + (4 + HB + 2 + a->len*HB)) {
            grow_buf(db, db->bufsz); /* 2*sz */
        }
        
        co = db->o;
        buf_int32(db->buf, co, lo);  /* prev->this */
        db->o += 4;
        buf_int32(db->buf, 0, co); /* set to 0 (for now) */
        
        buf_hash(db->buf, hash, db->o);
        db->o += HB;
        
        lho = db->o;    /* save offset; hash count goes here later */
        db->o += 2;
        hashct = 0;
        for (i=0; i<a->len; i++) {
            hashct++;
            buf_hash(db->buf, h_array_get(a, i), db->o); /* file hashes */
            db->o += HB;
            if (DEBUG_WD) fprintf(stderr, " %04x", h_array_get(a, i));
        }
        if (DEBUG_WD) fprintf(stderr, "\n");
        if (w->stop) { /* extremely common token -> skip it */
            fprintf(c->swlog, "%s\n", w->name); /* add to stop word log */
            if (DEBUG)
                fprintf(stderr, "Skipping stop word '%s', %u instances\n",
                    w->name, w->i);
            db->o = co; /* roll back */
        } else {
            buf_int16(db->buf, hashct, lho); /* hash count */
            if (DEBUG) fprintf(stderr, " -- hash count: %lu\n\n", hashct);
        }
        lo = co;
    }
    
    len = compress_buffer(db, xo + 4); /* +4: shift to include data length */
    if (DB_DEBUG) fprintf(stderr, "Adding compressed buffer length %lu (0x%04lx)\n", len, len);
    for (i=0; i<xo; i++) db->dbuf[i] = 'X';
    buf_int32(db->dbuf, len, xo); /* compressed byte count at head */
    return len + 4 + xo;
}


/**********
 * Tables *
 **********/

static void update_max_bufsize(int fd, int offset, ulong sz) {
    char buf[4];
    buf_int32(buf, sz, 0);
    /* TODO, when updating, compare */
    lseek(fd, offset, SEEK_SET);
    write(fd, buf, 4);
}

static void write_table_data(context *c, dbdata *db, int fd, table *t,
                             char *header, pack_fun *pack) {
    int i;
    /* buffer for offsets to buckets */
    uint bk_buf_sz = t->sz * 4;
    uint bk_buf_offset, len = strlen(header);
    char *bkbuf = alloc(bk_buf_sz, 'b');
    int xo;
    xo=DB_X_CT;
    
    /* Table format:
     * gln[F or T] [VERSION] [max buffer size/4] [byte offset of first table]
     *
     * Table format:
     * [absolute byte position of next table (or NULL)]
     * [table size/4]
     * [table of absolute offsets to each bucket's data/(bucket count * 4)]
     */
    /* FIXME: just truncate db for now; append later. */
    lseek(fd, 0, SEEK_SET);
    db->fo = 0;
    write(fd, header, len);
    write_int32(fd, 0);             /* max buffer size will go here later */
    db->fo = len + 4;
    
    write_int32(fd, db->fo + 4);    /* offset of first table */
    db->fo += 4;
    if (DB_DEBUG) fprintf(stderr, "Writing int32 for offset: %ld (0x%04lx)\n",
        db->fo + 4 + xo, db->fo + 4 + xo);
    
    write_int32(fd, 0);             /* next table offset -> NULL, for now */
    db->fo += 4;
    
    write_int32(fd, bk_buf_sz);     /* table size */
    db->fo += 4;
    if (DB_DEBUG) fprintf(stderr, "bk_buf_sz is %d (0x%04x)\n", bk_buf_sz, bk_buf_sz);
    
    lseek(fd, bk_buf_sz, SEEK_CUR);
    
    bk_buf_offset = db->fo;         /* will write offsets here later */
    db->fo += bk_buf_sz;
    
    /* for each bucket, all its info into a buffer, deflate it,
     * append to the database file, and set the offset to it. */
    for (i=0; i<t->sz; i++) {
        if (DB_DEBUG) fprintf(stderr, "-- bucket #%d\n", i);
        db->o = 0;
        len = pack(c, db, t->b[i]);
        if (0) dumphex(stderr, db->dbuf, len);
        write(fd, db->dbuf, len);
        if (DB_DEBUG) fprintf(stderr, "len (inc. header) is %d\n", len);
        
        buf_int32(bkbuf, db->fo, i * 4);
        if (DB_DEBUG) fprintf(stderr, "Wrote %ld (0x%04lx) to bkbuf at %d\n",
            db->fo, db->fo, i * 4);
        if (DB_DEBUG) fprintf(stderr, "Bucket %d packed to %u (0x%x) bytes "
            "starting at %lu (0x%04lx)\n\n",
            i, len, len, db->fo, db->fo);
        db->fo += len;
    }
    
    update_max_bufsize(fd, strlen(header), db->maxbufsz);
    lseek(fd, bk_buf_offset, SEEK_SET);
    
    if (DB_DEBUG) fprintf(stderr, "Max buffer size is %lu (0x%04lx).\n",
        db->maxbufsz, db->maxbufsz);
    write(fd, bkbuf, bk_buf_sz);
    if (DB_DEBUG) {
        fprintf(stderr, "\nOffset table:\n");
        dumphex(stderr, bkbuf, bk_buf_sz);
    }
    free(bkbuf);
}


/********
 * Main *
 ********/

int write_db(context *c) {
    dbdata *db = init_dbdata(c->fdb_fd, c->tdb_fd);
    init_zlib();
    
    /* TODO try opening existing file, else write it */
    /* write_file_header(); */
    if (DB_DEBUG) {
        fprintf(stderr, "File table:\n");
        table_stats(c->ft, 0);
        fprintf(stderr, "Token table:\n");
        table_stats(c->wt, 0);
    }
    
    write_table_data(c, db, db->ffd, c->ft, gln_file_header, pack_fname_bucket);
    write_table_data(c, db, db->tfd, c->wt, gln_token_header, pack_token_bucket);
    
#if PROFILE_COMPRESSION
    printf("Totals: IN: %lu\tOUT: %lu\t%.2f\n",
        db_bytes_in, db_bytes_out, db_bytes_out / (1.0*db_bytes_in));
#endif
    free_zlib();
    return 0;
}
