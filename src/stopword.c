#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <err.h>
#include <errno.h>
#include <time.h>
#include <sys/resource.h>

#include "glean.h"
#include "set.h"
#include "whash.h"
#include "gln_index.h"
#include "stopword.h"


/* Determine some stop words, based on frequency.
 *
 * The current method is to sort the array of words by descending
 * occurrence count, then iterate over the array and look for the point
 * at which the changes start to flatten out. (See: Zipf's law.)
 */

/* Array of pointers to all words, used for calculating stop words.
 * Should be a closure, maybe add a void* udata arg to set_apply. */
static uint w_ct;
static word **word_array = NULL;

static void add_word_to_array(void *v) { word_array[w_ct++] = (word *)v; }

/* Sort words by occurrence count, most to least. */
static int cmp_word_ct(const void *a, const void *b) {
    word **wa = (word **)a, **wb = (word **)b;
    uint ai = (*wa)->i, bi = (*wb)->i;
    return ai < bi ? 1 : ai > bi ? -1 : 0;
}

/* Step over the sorted word counts and find the index at
 * which the frequency starts to flatten out. */
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

/* Attempt to identify stopwords in the set<word>.
 * Each potential stopword has its word->stop flag set.
 * Returns <0 on error. */
int stopword_identify(set *word_set, ulong token_count,
                      ulong token_occurence_count, int verbose) {
    uint i, stop_at=0;
    word *w;
    
    if (DEBUG) printf("%lu tokens, %lu occurrences\n",
        token_count, token_occurence_count);
    word_array = alloc(sizeof(word *) * token_count, 'W');
    
    w_ct = 0;
    set_apply(word_set, add_word_to_array);
    if (w_ct == 0) return -1;
    qsort(word_array, token_count, sizeof(word *), cmp_word_ct);
    
    stop_at = find_plateau(STOPWORD_FLAT_CT, STOPWORD_CHANGE_FACTOR);
    if (DEBUG) fprintf(stderr, "stop_at = %d\n", stop_at);
    
    for (i=0; i<stop_at; i++) {
        w = word_array[i];
        w->stop = 1;
        if (verbose) fprintf(stderr, "Setting stopword #%d: %s (%d)\n",
            i, w->name, w->i);
    }
    return 0;
}
