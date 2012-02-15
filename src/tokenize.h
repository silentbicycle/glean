#ifndef TOKENIZE_H
#define TOKENIZE_H

/* Read file FN into set<word> S, then print every (word, count)
 * pair to stdout.*/
void tokenize_file(const char *fn, set *s, int case_sensitive);

#endif
