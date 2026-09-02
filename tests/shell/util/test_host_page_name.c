#include "host_page_name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(const char *name, bool value)
{
    if (!value) {
        fprintf(stderr, "FAIL: %s: expected true\n", name);
        exit(1);
    }
}

static void expect_false(const char *name, bool value)
{
    if (value) {
        fprintf(stderr, "FAIL: %s: expected false\n", name);
        exit(1);
    }
}

static void expect_eq_u8(const char *name, uint8_t expected, uint8_t actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected %u, got %u\n", name, expected, actual);
        exit(1);
    }
}

static void expect_streq(const char *name, const char *expected, const char *actual)
{
    if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0) {
        fprintf(
            stderr,
            "FAIL: %s: expected \"%s\", got \"%s\"\n",
            name,
            expected != NULL ? expected : "(null)",
            actual != NULL ? actual : "(null)");
        exit(1);
    }
}

static bool looks_like_stem(const char *stem)
{
    size_t i;

    if (stem == NULL || strlen(stem) != 15u) {
        return false;
    }
    for (i = 0; i < 15u; ++i) {
        if (i == 8u) {
            if (stem[i] != '-') {
                return false;
            }
        } else if (stem[i] < '0' || stem[i] > '9') {
            return false;
        }
    }
    return true;
}

static void test_stem_now_shape(void)
{
    char stem[16];

    expect_true("stem_now", host_page_name_stem_now(stem));
    expect_true("stem shape", looks_like_stem(stem));
    expect_false("stem_now null", host_page_name_stem_now(NULL));
}

static void test_build_path_xx_and_commit(void)
{
    host_page_name_state st;
    char path[256];
    char stem[16];
    char stem2[16];
    uint8_t xx;
    uint8_t xx2;
    char expected[256];

    memset(&st, 0, sizeof(st));

    expect_true(
        "build first",
        host_page_name_build_path(&st, "prints", "bmp", path, sizeof(path), stem, &xx));
    expect_eq_u8("first XX", 0u, xx);
    expect_true("first stem", looks_like_stem(stem));
    snprintf(expected, sizeof(expected), "prints/%s00.bmp", stem);
    expect_streq("first path", expected, path);

    /* build_path must not mutate state until commit */
    expect_eq_u8("seq untouched", 0u, st.seq);
    expect_true("stem empty until commit", st.last_stem[0] == '\0');

    host_page_name_commit(&st, stem, xx);
    expect_streq("committed stem", stem, st.last_stem);
    expect_eq_u8("committed seq", 0u, st.seq);

    expect_true(
        "build second",
        host_page_name_build_path(&st, "prints", "bmp", path, sizeof(path), stem2, &xx2));
    if (strcmp(stem2, stem) == 0) {
        expect_eq_u8("same-second XX", 1u, xx2);
        snprintf(expected, sizeof(expected), "prints/%s01.bmp", stem2);
        expect_streq("01 path", expected, path);
    } else {
        expect_eq_u8("new-second XX", 0u, xx2);
        snprintf(expected, sizeof(expected), "prints/%s00.bmp", stem2);
        expect_streq("new stem path", expected, path);
    }

    expect_false(
        "null dir",
        host_page_name_build_path(&st, NULL, "bmp", path, sizeof(path), stem, &xx));
    expect_false(
        "empty ext",
        host_page_name_build_path(&st, "prints", "", path, sizeof(path), stem, &xx));
    expect_false(
        "ext with implied use still needs non-empty",
        host_page_name_build_path(&st, "prints", NULL, path, sizeof(path), stem, &xx));
}

static void test_xx_exhaustion(void)
{
    host_page_name_state st;
    char path[256];
    char stem[16];
    char now[16];
    uint8_t xx;
    int attempt;

    memset(&st, 0, sizeof(st));
    for (attempt = 0; attempt < 5; ++attempt) {
        expect_true("stem for exhaust", host_page_name_stem_now(now));
        memcpy(st.last_stem, now, sizeof(st.last_stem));
        st.seq = 99u;
        if (!host_page_name_build_path(
                &st, "prints", "bmp", path, sizeof(path), stem, &xx)) {
            expect_true("exhausted", true);
            return;
        }
        /* Clock second advanced; accept XX=00 under new stem and retry. */
        expect_eq_u8("tick reset", 0u, xx);
        host_page_name_commit(&st, stem, xx);
    }
    fprintf(stderr, "FAIL: xx exhaustion not hit within retries\n");
    exit(1);
}

int main(void)
{
    test_stem_now_shape();
    test_build_path_xx_and_commit();
    test_xx_exhaustion();
    printf("host_page_name: ok\n");
    return 0;
}
