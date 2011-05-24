#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <err.h>
#include <regex.h>

#include "glean.h"
#include "ignore.h"

/* Read a list of regexes, return a linked list of them which are
 * rearranged in a least-recently-matched queue. */

typedef struct re_ll {
        regex_t *re;
        struct re_ll *next;
} re_ll;

re_group *ign_init_re_group() {
        re_group *g = alloc(sizeof(re_group), 'r');
        g->head = NULL;
        return g;
}

/* Add a compiled regex to an RE group. */
void ign_add_re(const char *pat, re_group *g) {
        regex_t *re = alloc(sizeof(regex_t), 'r');
        re_ll *ll = alloc(sizeof(re_ll), 'r');
        int ok;
        assert(pat != NULL);
        assert(g != NULL);
        if ((ok = regcomp(re, pat, REG_EXTENDED) != 0)) {
                fprintf(stderr, "Bad regex: %s\n", pat);
                err(1, "regcomp fail");
        }
        
        ll->re = re;
        ll->next = g->head;
        g->head = ll;
        if (DEBUG) printf("Added re for %s - %p\n", pat, (void *)ll);
}

void ign_free_re_group(re_group *g) {
        re_ll *h, *cur;
        assert(g);
        h = g->head;

        while (h) {
                cur = h;
                h = cur->next;
                regfree(cur->re);
                free(cur->re);
                free(cur);
        }
        free(g);
}

/* Try matching a string against a linked list of REs. On failure, return 0.
 * On success, return 1 and move that RE to the head. */
int ign_match(const char *str, re_group *g) {
        re_ll *r = NULL, *cur_head, *prev = NULL;
        regmatch_t pmatch;
        int ok = 1, ct=0;

        assert(str); assert(g);
        cur_head = g->head;

        for (r = g->head; r != NULL; r = r->next) {
                if (DEBUG)
                        fprintf(stderr, "Trying %s against %d (%p) - ", str, ct,
                            (void *)r);

                ok = regexec(r->re, str, 1, &pmatch, 0);
                if (ok == 0) {
                        if (DEBUG) fprintf(stderr, "success");
                        break;
                }

                if (DEBUG) { fprintf(stderr, "failure"); ct++; }
                prev = r;
        }

        if (ok == 0) {
                if (prev) {
                        prev->next = r->next;
                        g->head = r;
                        r->next = cur_head;
                }
                return 1;
        }

        return 0;
}
