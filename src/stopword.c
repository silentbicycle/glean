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
#include "word.h"
#include "gln_index.h"
#include "stopword.h"

/* Determine some stop words, based on frequency.
 *
 * The current method is to sort the array of words by descending
 * occurrence count, then iterate over the array and look for the point
 * at which the changes start to flatten out. (See: Zipf's law.)
 */

/* Userdata/closure struct for stopword_identify's set_apply. */
typedef struct sw_udata {
    uint word_count;
    word **word_array;
} sw_udata;

static void word_add_to_array(void *v, void *udata) {
    sw_udata *ud = (sw_udata *) udata;
    word *w = (word *) v;
    ud->word_array[ud->word_count++] = w;
}

/* Sort words by occurrence count, most to least. */
static int cmp_word_ct(const void *a, const void *b) {
    word **wa = (word **)a, **wb = (word **)b;
    uint ai = (*wa)->count, bi = (*wb)->count;
    return ai < bi ? 1 : ai > bi ? -1 : 0;
}

/* Step over the sorted word counts and find the index at
 * which the frequency starts to flatten out. */
static uint find_plateau(word **word_array, uint word_count,
                         int flat_ct, double change_factor) {
    uint i, last, diff, lastdiff=1, flats=0, stop_at=0;
    word *w;
    double chg;
    
    w = word_array[0];
    lastdiff = diff = last = w->count;
    for (i=0; i<word_count; i++) {
        w = word_array[i];
        diff = abs(last - w->count);
        chg = lastdiff / (1.0*diff);
        
        if (DEBUG) fprintf(stderr, "%d\tld %d, d %d\tchg=%f\t%d\n",
            i, lastdiff, diff, chg, flats);
        if (chg < change_factor || diff == 0) {
            flats++;
            if (flats > flat_ct) { stop_at = i; break; }
        } else if (flats > 0) {
            flats--;
        }
        last = w->count;
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
    sw_udata ud;
    word **word_array = alloc(sizeof(word *) * token_count, 'W');
    
    if (DEBUG) printf("%lu tokens, %lu occurrences\n",
        token_count, token_occurence_count);
    
    ud.word_count = 0;
    ud.word_array = word_array;
    set_apply(word_set, word_add_to_array, &ud);
    if (ud.word_count == 0) return -1;

    qsort(ud.word_array, token_count, sizeof(word *), cmp_word_ct);
    
    stop_at = find_plateau(word_array, ud.word_count,
        STOPWORD_FLAT_CT, STOPWORD_CHANGE_FACTOR);
    if (DEBUG) fprintf(stderr, "stop_at = %d\n", stop_at);
    
    for (i=0; i<stop_at; i++) {
        w = ud.word_array[i];
        w->stop = 1;
        if (verbose) fprintf(stderr, "Setting stopword #%d: '%s' (count %d)\n",
            i, w->name, w->count);
    }
    return 0;
}
