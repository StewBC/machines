#pragma once

#include <stdbool.h>

struct runtime;

/* Soft-attach MPS-803-class printer on the machine. output_dir may be NULL/empty
   when disabling. Enabling with a bad/empty dir fails soft (log + leave prior).
   Format is fixed BMP for v1. */
bool runtime_printer_set_enabled(
    struct runtime *rt,
    bool enabled,
    const char *output_dir);

/* Force-flush dirty page to host file; no-op if disabled / !dirty / blank. */
void runtime_printer_force_flush(struct runtime *rt);
