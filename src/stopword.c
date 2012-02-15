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
 * which the frequency starts to flatten out.
 *
 * TODO: Still experimental. */
static uint find_plateau(word **word_array, uint word_count,
                         uint token_occurence_count,
                         int flat_ct, double change_factor) {
    uint flats=0, stop_at=0;
    word *w = NULL;
    double lastperc = 100.0;
    
    if (DEBUG)
        fprintf(stderr, " -- word count %u\n", word_count);

    w = word_array[0];

    for (uint i=0; i<word_count; i++) {
        w = word_array[i];
        double perc = (100.0 * w->count)/token_occurence_count;
        double perc_change = lastperc - perc;
        
        if (DEBUG) fprintf(stderr, "%d\t%s\t%.3f\t%.3f\t->\t%.3f (%d)\n",
            i, w->name, lastperc, perc, perc_change, flats);
        if (perc_change < change_factor) {
            flats++;
            if (flats > flat_ct) { stop_at = i; break; }
        } else if (flats > 0) {
            flats--;
        }
        lastperc = perc;
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
    double cum = 0;
    
    if (DEBUG) printf("%lu tokens, %lu occurrences\n",
        token_count, token_occurence_count);
    
    ud.word_count = 0;
    ud.word_array = word_array;
    set_apply(word_set, word_add_to_array, &ud);
    if (ud.word_count == 0) return -1;

    qsort(ud.word_array, token_count, sizeof(word *), cmp_word_ct);
    
    stop_at = find_plateau(word_array, ud.word_count,
        token_occurence_count,
        STOPWORD_FLAT_CT, STOPWORD_CHANGE_FACTOR);
    if (DEBUG) fprintf(stderr, "stop_at = %d\n", stop_at);
    
    for (i=0; i<stop_at; i++) {
        w = ud.word_array[i];
        double perc = (100.0 * w->count)/token_occurence_count;
        if (perc < STOPWORD_MIN_PERCENT) break;
        cum += perc;
        if (verbose) fprintf(stderr, " -- Setting stopword '%s' "
            "(%.2f%% of input, %.2f%% cumulative)\n",
            w->name, perc, cum);
        w->stop = 1;
    }
    return 0;
}
