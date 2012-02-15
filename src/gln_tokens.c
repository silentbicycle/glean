#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <err.h>
#include <poll.h>

#include "glean.h"
#include "set.h"
#include "whash.h"
#include "tokenize.h"

#define BUF_SZ 4096
#define TIMEOUT 60              /* just die after 1 minute idle */
#define FLUSH_COUNT 100         /* clear table & shrink every N files */

/* Read in filenames from stdin and tokenize them. If given " DONE", quit. */
int main(int argc, char *argv[]) {
    table *wt = init_word_table(0);
    char buf[BUF_SZ];
    int pid = getpid();
    int case_sensitive = (argc == 2 && strcmp(argv[1], "-c") == 0);
    struct pollfd fds[1];
    int res = 0, files = 0;
    
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    
    for (;;) {
        res = poll(fds, 1, TIMEOUT * 1000);
        if (res == -1) err(1, "poll");
        else if (res == 0) exit(1); /* timeout, gln_index probably died */
        else assert(fds[0].revents & POLLIN);
        
        if (fgets(buf, BUF_SZ, stdin) != NULL) {
            buf[strcspn(buf, "\n")] = '\0';
            if (strcmp(buf, " DONE") == 0) break;
            if (DEBUG) fprintf(stderr, "-- %d: Got filename %s...\n",
                pid, buf);
            tokenize_file(buf, wt, case_sensitive);
            fflush(stdout);
            
            if (++files >= FLUSH_COUNT) {
                table_free(wt, free_word);
                wt = init_word_table(0);
                if (0) fprintf(stderr, "flush: %d, %d\n", getpid(), files);
                files = 0;
            }
            
            if (DEBUG) fprintf(stderr, "done\n");
            if (DEBUG) fprintf(stderr, "-- Finished filename %s\n", buf);
        } else break;
    }
    if (DEBUG_HASH) table_stats(wt, 0);
    table_free(wt, free_word);
    return 0;
}
