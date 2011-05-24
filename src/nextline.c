#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "glean.h"
#include "alloc.h"

/* Ugly portability wrapper for getline / fgetln. */
#if !defined(HAS_GETLINE) && !defined(HAS_FGETLN)
#if defined(__linux__) || defined(__CYGWIN__)
#define HAS_GETLINE
#elif defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
        defined(__APPLE__) || defined(__SHROVIS_BISHOPTHORPE__)
#define HAS_FGETLN
#else
/* __sun__? */
#error Must have getline or fgetln.
#endif
#endif /* !defined(HAS_GETLINE) && !defined(HAS_FGETLN) */

#define BUF_SZ 4096

#if defined(HAS_GETLINE)
static char *buf = NULL;
static long buf_sz = BUF_SZ;
#endif

char *nextline(FILE *f, size_t *len) {

#if defined(HAS_GETLINE)
        /* This is ugly, could probably be made better. */
        size_t sz = buf_sz - 1;
        char *line = NULL;
        ssize_t res = getline(&line, &sz, f);

        if (buf == NULL) buf = (char *) alloc(BUF_SZ, 'b');

        if (res == -1) {
                if (line) free(line);
                return NULL;
        } else if (res > buf_sz) {
                free(buf);
                buf_sz = res + 1;
                if (DEBUG) fprintf(stderr, "nextline: buf_sz %ld\n", buf_sz);
                buf = alloc(buf_sz, 'b');
        }
        *len = res;
        strncpy(buf, line, *len);
        buf[res > 0 ? res-1 : 0] = '\0';
        free(line);
        return buf;

#elif defined(HAS_FGETLN)
        /* SO much simpler! */
        return fgetln(f, len);
#endif
}

void free_nextline_buffer() {
#if defined(HAS_GETLINE)
        free(buf);
#endif
}
