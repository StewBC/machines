#include "window_title.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_eq(const char *name, const char *expected, const char *actual)
{
    if (strcmp(expected, actual) != 0) {
        fprintf(stderr, "%s: expected `%s`, got `%s`\n", name, expected, actual);
        exit(1);
    }
}

int main(void)
{
    char actual[96];

    debugger_format_window_title(
        actual, sizeof(actual), "a2m", "//e Enhanced", "1.00 MHz", "Running");
    expect_eq("a2m running", "a2m - //e Enhanced - 1.00 MHz - Running", actual);

    debugger_format_window_title(
        actual, sizeof(actual), "a2m", "][+ ", "Max", "Inspect");
    expect_eq("a2m inspect", "a2m - ][+  - Max - Inspect", actual);

    debugger_format_window_title(
        actual, sizeof(actual), "c64m", "PAL", "Normal", "Running");
    expect_eq("c64m pal", "c64m - PAL - Normal - Running", actual);

    debugger_format_window_title(
        actual, sizeof(actual), "c64m", "PAL", "Normal", "Inspect");
    expect_eq("c64m inspect", "c64m - PAL - Normal - Inspect", actual);

    return 0;
}
