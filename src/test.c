#include <stdlib.h>
#include <stdio.h>

#include "greatest.h"

extern SUITE(array_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(array_suite);
    GREATEST_MAIN_END();
}
