#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <time.h>
#include <sys/resource.h>

#include "glean.h"
#include "hash.h"
#include "whash.h"
#include "gln_index.h"
#include "stopword.h"


/* Determine some stop words, based on frequency.
 *
 * The current method is to sort the array of words by descending
 * occurrence count, then iterate over the array and look for the point
 * at which the changes start to flatten out.
 */

/* Array of pointers to all words, used for calculating stop words.
 * Should be a closure, maybe add a void* udata arg to table_apply. */
static uint w_ct;
static word **word_array = NULL;

static void add_word_to_array(void *v) { word_array[w_ct++] = (word *)v; }

/* Sort words by occurrence count, most to least. */
static int cmp_word_ct(const void *a, const void *b) {
    word **wa = (word **)a, **wb = (word **)b;
    uint ai = (*wa)->i, bi = (*wb)->i;
    return ai < bi ? 1 : ai > bi ? -1 : 0;
}

static uint find_plateau(int flat_ct, double change_factor) {
    uint i, last, diff, lastdiff=1, flats=0, stop_at=0;
    word *w;
    double chg;
    
    w = word_array[0];
    lastdiff = diff = last = w->i;
    for (i=0; i<w_ct; i++) {
        w = word_array[i];
        diff = abs(last - w->i);
        chg = lastdiff / (1.0*diff);
        
        if (DEBUG) fprintf(stderr, "%d\tld %d, d %d\tchg=%f\t%d\n",
            i, lastdiff, diff, chg, flats);
        if (chg < change_factor || diff == 0) {
            flats++;
            if (flats > flat_ct) { stop_at = i; break; }
        } else if (flats > 0) {
            flats--;
        }
        last = w->i;
        lastdiff = diff;
    }
    return stop_at;
}

void identify_stop_words(context *c) {
    uint i, stop_at=0;
    word *w;
    
    if (DEBUG || 1) printf("%lu tokens, %lu occurrences\n", c->tct, c->toct);
    word_array = alloc(sizeof(word *) * c->tct, 'W');
    
    w_ct = 0;
    table_apply(c->wt, add_word_to_array);
    if (w_ct == 0) return;
    qsort(word_array, c->tct, sizeof(word *), cmp_word_ct);
    
    stop_at = find_plateau(FLAT_CT, CHANGE_FACTOR);
    if (DEBUG) fprintf(stderr, "stop_at = %d\n", stop_at);
    
    for (i=0; i<stop_at; i++) {
        w = word_array[i];
        w->stop = 1;
        if (DEBUG) fprintf(stderr, "Setting stopword #%d: %s (%d)\n", i, w->name, w->i);
    }
}
