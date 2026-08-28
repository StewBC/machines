#include "memory_source.h"

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", \
                    __FILE__, __LINE__, #expr); \
            failures++; \
        } \
    } while (0)

static const memory_source k_memview[] = {
    { 0u, "Map", "map", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 1u, "Main", "main", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 3u, "Aux", "aux", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 4u, "LC1", "lc1", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 5u, "LC2", "lc2", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 2u, "ROM", "rom", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII }
};

static const memory_source k_disasm[] = {
    { 0u, "Map", "map", 0u, 0x10000u, MEMSRC_WRITABLE },
    { 2u, "ROM", "rom", 0u, 0x10000u, 0u },
    { 1u, "Main", "main", 0u, 0x10000u, MEMSRC_WRITABLE }
};

static const memory_source k_c64_mem[] = {
    { 0u, "CPU map", "map", 0u, 0x10000u, MEMSRC_WRITABLE },
    { 2u, "ROM", "rom", 0u, 0x10000u, 0u },
    { 1u, "RAM", "ram", 0u, 0x10000u, MEMSRC_WRITABLE },
    { 3u, "Drive 8", "drive8", 0u, 0x10000u, MEMSRC_FOREIGN_BUS },
    { 4u, "Drive 9", "drive9", 0u, 0x10000u, MEMSRC_FOREIGN_BUS }
};

int main(void)
{
    uint32_t id;

    id = memory_source_cycle_next(k_memview, 6, 0u);
    CHECK(id == 1u);
    id = memory_source_cycle_next(k_memview, 6, 1u);
    CHECK(id == 3u);
    id = memory_source_cycle_next(k_memview, 6, 2u);
    CHECK(id == 0u);

    id = memory_source_cycle_next(k_disasm, 3, 0u);
    CHECK(id == 2u);
    id = memory_source_cycle_next(k_disasm, 3, 2u);
    CHECK(id == 1u);
    id = memory_source_cycle_next(k_disasm, 3, 1u);
    CHECK(id == 0u);

    id = memory_source_cycle_next(k_c64_mem, 5, 0u);
    CHECK(id == 2u);
    id = memory_source_cycle_next(k_c64_mem, 5, 1u);
    CHECK(id == 3u);
    id = memory_source_cycle_next(k_c64_mem, 5, 4u);
    CHECK(id == 0u);

    CHECK(memory_source_cycle_next(k_memview, 6, 99u) == 0u);
    CHECK(memory_source_cycle_next(NULL, 0, 0u) == 0u);

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
