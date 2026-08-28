#include "control_protocol.h"
#include "memory_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", \
                    __FILE__, __LINE__, #expr); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    size_t n = 0;
    const memory_source *src = c64_memory_sources(&n);
    uint32_t id;

    CHECK(src != NULL);
    CHECK(n == 5u);
    CHECK(strcmp(src[0].token, "map") == 0);
    CHECK((src[0].flags & MEMSRC_HIGHBIT_ASCII) == 0u);
    CHECK((src[3].flags & MEMSRC_FOREIGN_BUS) != 0u);

    /* UI Opt+M order (not control-table declaration order): map, rom, ram, drive8, drive9. */
    {
        static const memory_source cycle[] = {
            { 0u, "CPU map", "map", 0u, 0x10000u, MEMSRC_WRITABLE },
            { 2u, "ROM", "rom", 0u, 0x10000u, 0u },
            { 1u, "RAM", "ram", 0u, 0x10000u, MEMSRC_WRITABLE },
            { 3u, "Drive 8", "drive8", 0u, 0x10000u, MEMSRC_FOREIGN_BUS },
            { 4u, "Drive 9", "drive9", 0u, 0x10000u, MEMSRC_FOREIGN_BUS }
        };
        id = memory_source_cycle_next(cycle, 5, 0u);
        CHECK(id == 2u);
        id = memory_source_cycle_next(cycle, 5, 2u);
        CHECK(id == 1u);
        id = memory_source_cycle_next(cycle, 5, 1u);
        CHECK(id == 3u);
        id = memory_source_cycle_next(cycle, 5, 3u);
        CHECK(id == 4u);
        id = memory_source_cycle_next(cycle, 5, 4u);
        CHECK(id == 0u);
    }

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
