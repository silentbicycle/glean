#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "glean.h"
#include "eta.h"

#include "greatest.h"

static long cur_secs() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) assert(0);
    return tv.tv_sec;
}

/* 0 files to go -> "0s " */
TEST eta_0() {
    char buf[1024];
    int len = eta_tostring(buf, 1024, cur_secs(), 100, 100);
    ASSERT_EQ(3, len);
    ASSERT_STR_EQ("0s ", buf);
    PASS();
}

/* Half done, started 10 sec ago -> "10s " */
TEST eta_half_done() {
    char buf[1024];
    long secs = cur_secs();
    int len = eta_tostring(buf, 1024, secs - 10, 50, 100);
    ASSERT_EQ(4, len);
    ASSERT_STR_EQ("10s ", buf);
    PASS();
}

TEST eta_wont_fit() {
    char tiny_buf[1];
    long secs = cur_secs();
    int len = eta_tostring(tiny_buf, 1, secs - 10, 50, 100);
    ASSERT_EQ(-1, len);
    PASS();
}

/* Verify that starting 3 hours, 4 minutes, and 10 seconds ago and
 * being halfway done gets "3h 4m 10s ". */
TEST eta_3h_4m_10s() {
    char buf[1024];
    long secs = cur_secs();
    long ago = (3 * 3600) + (4 * 60) + 10;
    int len = eta_tostring(buf, 1024, secs - ago, 50, 100);
    char *exp = "3h 4m 10s ";
    ASSERT_EQ(strlen(exp), len);
    ASSERT_STR_EQ(exp, buf);
    PASS();
}

SUITE(eta_suite) {
    RUN_TEST(eta_0);
    RUN_TEST(eta_half_done);
    RUN_TEST(eta_wont_fit);
    RUN_TEST(eta_3h_4m_10s);
}
