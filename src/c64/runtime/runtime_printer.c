#include "runtime_printer.h"

#include "host_log.h"
#include "host_page_writer.h"
#include "runtime_internal.h"

#include <string.h>

bool runtime_printer_set_enabled(
    runtime *rt,
    bool enabled,
    const char *output_dir)
{
    c64_printer *printer;

    if (rt == NULL) {
        return false;
    }

    printer = &rt->machine.printer;

    if (!enabled) {
        c64_printer_set_enabled(printer, false);
        return true;
    }

    if (output_dir == NULL || output_dir[0] == '\0') {
        log_error("printer: enable refused (empty output_dir)");
        runtime_publish_error(rt, "Printer enable refused: empty output directory");
        return false;
    }

    if (!host_page_writer_ensure_dir(output_dir)) {
        log_error("printer: enable refused (cannot create %s)", output_dir);
        runtime_publish_error(rt, "Printer enable refused: cannot create output directory");
        return false;
    }

    c64_printer_set_output_dir(printer, output_dir);
    c64_printer_set_format_bmp(printer);
    c64_printer_set_enabled(printer, true);
    return true;
}
