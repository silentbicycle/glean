#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "glean.h"
#include "array.h"

#include "greatest.h"

TEST h_append_and_check() {
    h_array *a = h_array_init(1);

    ASSERT_EQ(0, h_array_length(a));

    for (int i=0; i<1000; i++) {
        h_array_append(a, i);
    }
    ASSERT_EQ(1000, h_array_length(a));

    for (int i=0; i<1000; i++) {
        ASSERT_EQ(i, h_array_get(a, i));
    }
    h_array_free(a);
    PASS();
}

TEST h_sort() {
    h_array *a = h_array_init(1);

    for (int i=0; i<1000; i++) {
        h_array_append(a, 999 - i);
    }
    h_array_sort(a);

    for (int i=0; i<1000; i++) {
        ASSERT_EQ(i, h_array_get(a, i));
    }
    h_array_free(a);
    PASS();
}

TEST h_uniq() {
    h_array *a = h_array_init(1);

    for (int i=0; i<100; i++) {
        for (int ct=0; ct<i; ct++) h_array_append(a, i);
    }
    h_array_uniq(a);
    int len = h_array_length(a);

    for (int i=0; i<len; i++) {
        ASSERT_EQ(i + 1, h_array_get(a, i));
    }
    h_array_free(a);
    PASS();
}

#define AP(ARR, V) h_array_append(ARR, V)
#define EXP(ARR, I, V) ASSERT_EQ(V, h_array_get(ARR, I))

TEST h_union() {
    h_array *a = h_array_init(10);
    h_array *b = h_array_init(10);
    for (int i=0; i<10; i++) {
        AP(a, i);
        AP(b, 2*i);
    }

    h_array *u = h_array_union(a, b);
    int len = h_array_length(u);
    ASSERT_EQ(15, len);

    for (int i=0; i<=10; i++) EXP(u, i, i);
    EXP(u, 11, 12);
    EXP(u, 12, 14);
    EXP(u, 13, 16);
    EXP(u, 14, 18);

    h_array_free(a);
    h_array_free(b);
    h_array_free(u);
    PASS();
}

TEST h_intersection() {
    h_array *a = h_array_init(10);
    h_array *b = h_array_init(10);
    for (int i=0; i<10; i++) {
        AP(a, i);
        AP(b, i + 5);
    }

    h_array *u = h_array_intersection(a, b);
    int len = h_array_length(u);

    ASSERT_EQ(5, len);

    for (int i=0; i<len; i++) EXP(u, i, i + 5);

    h_array_free(a);
    h_array_free(b);
    h_array_free(u);
    PASS();
}

TEST h_complement() {
    h_array *a = h_array_init(10);
    h_array *b = h_array_init(10);
    for (int i=0; i<10; i++) {
        AP(a, i);
        AP(b, i + 5);
    }

    h_array *u = h_array_complement(a, b);
    int len = h_array_length(u);

    ASSERT_EQ(5, len);
    for (int i=0; i<len; i++) EXP(u, i, i);

    h_array_free(a);
    h_array_free(b);
    h_array_free(u);
    PASS();
}

TEST v_append_and_check() {
    v_array *a = v_array_init(1);

    ASSERT_EQ(0, v_array_length(a));

    for (uintptr_t i=0; i<1000; i++) {
        v_array_append(a, (void *) i);
    }
    ASSERT_EQ(1000, v_array_length(a));

    for (uintptr_t i=0; i<1000; i++) {
        ASSERT_EQ((void *) i, v_array_get(a, i));
    }
    v_array_free(a, NULL);
    PASS();
}

static uintptr_t checked[1000]; /* static -> initialized to 0 */

static void free_cb(void *v) {
    checked[(uintptr_t) v] = 1;
}

static int cmp_cb(const void *a, const void *b) {
    uintptr_t va = *(uintptr_t *) a;
    uintptr_t vb = *(uintptr_t *) b;
    int res = va < vb ? -1 : va > vb ? 1 : 0;
    return res;
}

TEST v_sort() {
    v_array *a = v_array_init(1);
    for (uintptr_t i=0; i<100; i++) {
        v_array_append(a, (void *) (99 - i));
    }
    int len = v_array_length(a);
    ASSERT_EQ(100, len);

    v_array_sort(a, cmp_cb);
    for (uintptr_t i=0; i<100; i++) {
        uintptr_t v = (uintptr_t) v_array_get(a, i);
        ASSERT_EQ(i, v);
    }
    PASS();
}

TEST v_free_cb_test() {
    v_array *a = v_array_init(1);

    for (uintptr_t i=0; i<1000; i++) {
        v_array_append(a, (void *) i);
    }
    ASSERT_EQ(1000, v_array_length(a));

    for (uintptr_t i=0; i<1000; i++) {
        ASSERT_EQ((void *) i, v_array_get(a, i));
    }

    v_array_free(a, free_cb);
    for (uintptr_t i=0; i<1000; i++) {
        ASSERT_EQ(1, checked[i]);
    }

    PASS();
}


SUITE(array_suite) {
    RUN_TEST(h_append_and_check);
    RUN_TEST(h_sort);
    RUN_TEST(h_uniq);
    RUN_TEST(h_union);
    RUN_TEST(h_intersection);
    RUN_TEST(h_complement);
    RUN_TEST(v_append_and_check);
    RUN_TEST(v_sort);
    RUN_TEST(v_free_cb_test);
}
