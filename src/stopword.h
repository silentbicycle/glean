#ifndef STOPWORD_H
#define STOPWORD_H

/* If the difference between the last word's corpus percentage
 * and the current word's is < this, consider the frequency curve
 * to be flattening out. 0.10 == 0.1% difference */
#define STOPWORD_CHANGE_FACTOR 0.10

/* If using stop words, how many relatively-flat differences are
 * needed before stopping. */
#define STOPWORD_FLAT_CT 5

/* A wold must be at least this percent of the corpus to be a stopword. */
#define STOPWORD_MIN_PERCENT 0.5

/* Attempt to identify stopwords in the set<word>.
 * Each potential stopword has its word->stop flag set.
 * Returns <0 on error. */
int stopword_identify(set *word_set, ulong token_count,
    ulong token_occurence_count, int verbose);

#endif
