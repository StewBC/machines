#include "runtime_printer.h"

#include "host_log.h"
#include "runtime_internal.h"

bool runtime_printer_configure(runtime *rt, const char *output_dir)
{
    if (rt == NULL) {
        return false;
    }

    if (output_dir == NULL || output_dir[0] == '\0') {
        log_error("printer: configure refused (empty output_dir)");
        return false;
    }

    apple2_set_printer_output_dir(&rt->machine, output_dir);
    return true;
}

void runtime_printer_force_flush(runtime *rt)
{
    if (rt == NULL) {
        return;
    }
    apple2_imagewriter_force_flush(&rt->machine);
}

void runtime_printer_pre_cold_reset_flush(runtime *rt)
{
    if (rt == NULL) {
        return;
    }
    /* Unchanged SSC slots skip detach; flush before cold reset discards. */
    apple2_imagewriter_force_flush(&rt->machine);
}
