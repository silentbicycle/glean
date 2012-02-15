#ifndef STOPWORD_H
#define STOPWORD_H

/* Attempt to identify stopwords in the set<word>.
 * Each potential stopword has its word->stop flag set.
 * Returns <0 on error. */
int stopword_identify(set *word_set, ulong token_count,
    ulong token_occurence_count, int verbose);

#endif
