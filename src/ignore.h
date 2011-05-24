#ifndef IGNORE_H
#define IGNORE_H

typedef struct re_group {
        struct re_ll *head;
} re_group;

re_group *ign_init_re_group(void);
void ign_free_re_group(re_group *g);
void ign_add_re(const char *pat, re_group *g);
int ign_match(const char *str, re_group *g);

#endif
