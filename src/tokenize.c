/* 
 * Copyright (c) 2010 Scott Vokes <vokes.s@gmail.com>
 *  
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <err.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "glean.h"
#include "array.h"
#include "hash.h"
#include "whash.h"
#include "tokenize.h"

#define BUF_SZ 64 * 1024

static char buf[BUF_SZ];

static off_t getsz(int id) {
        struct stat sb;
        if (fstat(id, &sb) == -1) err(1, "getsz fail");
        return sb.st_size;
}

/* Based on the first read, is the file mostly textual?
 * If the first read is really small, just assume it's ok. */
static int is_mostly_binary(size_t ct, char *buf) {
        int i, ok=0;
        if (ct < 100) return 0;

        for (i=0; i<ct; i++) if (isalnum(buf[i]) || isspace(buf[i]) || ispunct(buf[i])) ok++;
        if (DEBUG) fprintf(stderr, "is_mostly_binary: %d %ld %f -> %d\n",
            ok, ct, ok / (ct * 1.0), (ok / (ct * 1.0)) < MIN_TOKEN_PRINTABLE);
        return (ok / (ct * 1.0)) < MIN_TOKEN_PRINTABLE;
}

/* Loop over the file, reading a chunk at a time, saving every known word. */
static int readloop(int fd, off_t len, table *wt, int case_sensitive) {
        int i, j, last=0, inword=0;
        size_t ct=0, read_sz, read_offset;
        size_t off=0, last_off=0; /* file offset and offset of last token */
        char c;                   /* current byte */
        int alf, diff; /* isalpha(c) flag; diff */
        uint tokens = 0;

        for (;;) {
                read_sz = BUF_SZ; read_offset = 0;

                /* If in the middle of a word, prepend the remainder to the
                 * next buffer read and adjust lengths accordingly. */
                if (inword) {
                        assert(ct);
                        diff = ct - last;
                        if (DEBUG) fprintf(stderr, "Copying incomplete word (%d): %s\n",
                            diff, buf + last);
                        strncpy(buf, buf + last, diff);
                        read_offset = diff;
                        read_sz -= read_offset;
                        last = 0;
                }

                ct = read(fd, buf + read_offset, read_sz);
                if (DEBUG) fprintf(stderr, "-- read %ld more %d\n", ct, inword);
                if (ct < 1) break;

                /* FIXME: Really, this check is still useful after the first X kb,
                 * but number of valid words may be a more useful metric.
                 * Also: could also fork "file -f - -i" here, very expensive though. */
                if (off == 0 && is_mostly_binary(ct, buf)) return 1;

                last = 0;

                /* Scan along and save each consecutive alphabetical region. */
                for (i=0; i<ct; i++) {
                        c = buf[i];
                        alf = isalpha(c);

                        if (inword && !alf) {     /* end of current token */
                                inword = 0; diff = i - last;
                                if (diff >= MIN_WORD_SZ && diff < MAX_WORD_SZ) {
                                        if (!case_sensitive)
                                                for (j=0; j<diff; j++)
                                                        buf[last+j] = tolower(buf[last+j]);
                                        add_word(wt, buf + last, diff);
                                }
                                last_off = off;
                                tokens++;
                        } else if (!inword && alf) { /* start of new token */
                                last = i;
                                inword = 1;
                        }
                        off++;
                }
        }

        if (ct == -1) err(1, "read fail");
        print_and_zero_words(wt);
        return 0;
}

void tokenize_file(const char *fn, table *wt, int case_sensitive) {
        int fd = open(fn, O_RDONLY, 0);
        off_t len;
        int skipped, res;

        if (fd == -1) {         /* warn and skip it */
                perror(fn);
                printf(" SKIP\n");
        } else {
                len = getsz(fd);
                skipped = readloop(fd, len, wt, case_sensitive);
                res = close(fd);
                assert(res == 0);
                printf(skipped ? " SKIP\n" : " DONE\n");
        }
}
