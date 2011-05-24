#include <stdlib.h>
#include <stdio.h>
#include <check.h>

START_TEST (foo_test) {
        int x = 1+1;
}
END_TEST

Suite *foo_suite(void) {
        Suite *s = suite_create("Foo_suite");
        TCase *tc_foo = tcase_create("Foo_test");
        tcase_add_test(tc_foo, foo_test);
        suite_add_tcase(s, tc_foo);
        return s;
}


int main(int argc, char **argv) {
        Suite *s = foo_suite();
        SRunner *sr = srunner_create(s);
        int fail_ct;
        srunner_run_all(sr, CK_VERBOSE);
        fail_ct = srunner_ntests_failed(sr);
        srunner_free(sr);
        return (fail_ct == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
