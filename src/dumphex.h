#ifndef DUMPHEX_H
#define DUMPHEX_H

/* Print a hexdump of the first LEN bytes of BUF to FILE F. */
void dumphex(FILE *f, char *buf, int len);

#endif
