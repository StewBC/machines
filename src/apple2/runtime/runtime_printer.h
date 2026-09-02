#pragma once

#include <stdbool.h>

struct runtime;

/* Apply printer output_dir when an SSC is (or will be) installed. Presence is
   the slot card — there is no soft-power enable. Empty/NULL dir soft-fails. */
bool runtime_printer_configure(struct runtime *rt, const char *output_dir);

/* Force-flush dirty ImageWriter page (no-op if no SSC / clean / sealed). */
void runtime_printer_force_flush(struct runtime *rt);

/* Configure Apply: flush dirty IW before cold reset when SSC remains. */
void runtime_printer_pre_cold_reset_flush(struct runtime *rt);
