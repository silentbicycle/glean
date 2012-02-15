#include <stdio.h>
#include <ctype.h>

/* Print a hexdump of the first LEN bytes of BUF to FILE F. */
void dumphex(FILE *f, char *buf, int len) {
    int i, j;
    char c;
    for (i=0; i<len; i++) {
        fprintf(f, "%02x", buf[i] & 0xff);
        if (i % 2 == 1) fprintf(f, " ");
        if (i % 16 == 15) {
            fprintf(f, " ");
            for (j=15; j>=0; j--) {
                c = buf[i-j];
                fprintf(f, "%c", (!isascii(c) || !isprint(c) ? '.' : c));
            }
            fprintf(f, "\n");
        }
    }
    for (j=i%16; j<16; j++) {
        fprintf(f, "  ");
        if (j % 2 == 1) fprintf(f, " ");
    }
    fprintf(f, " ");
    for (j=len-(i%16); j<len; j++) {
        c = buf[j];
        fprintf(f, "%c", (!isascii(c) || !isprint(c) ? '.' : c));
    }
    fprintf(f, "\n");
}
