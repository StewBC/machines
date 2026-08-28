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

static const memory_source k_host[] = {
    { 0u, "Map", "map", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 1u, "Main", "main", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 3u, "Aux", "aux", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE }
};

static const memory_source k_foreign[] = {
    { 0u, "CPU map", "map", 0u, 0x10000u, MEMSRC_WRITABLE },
    { 3u, "Drive 8", "drive8", 0u, 0x10000u, MEMSRC_FOREIGN_BUS },
    { 4u, "Drive 9", "drive9", 0u, 0x10000u, MEMSRC_FOREIGN_BUS }
};

int main(void)
{
    const memory_source *src;

    src = memory_source_find_by_token(k_host, 3, "aux");
    CHECK(src != NULL);
    CHECK(src->id == 3u);
    CHECK((src->flags & MEMSRC_HIGHBIT_ASCII) != 0u);
    CHECK((src->flags & MEMSRC_FOREIGN_BUS) == 0u);

    src = memory_source_find_by_id(k_foreign, 3, 3u);
    CHECK(src != NULL);
    CHECK(strcmp(src->token, "drive8") == 0);
    CHECK((src->flags & MEMSRC_FOREIGN_BUS) != 0u);
    CHECK((src->flags & MEMSRC_HIGHBIT_ASCII) == 0u);

    /* Same numeric id is not the same bus: host 3 is Aux, foreign 3 is Drive 8. */
    CHECK(memory_source_find_by_id(k_host, 3, 3u)->flags !=
          memory_source_find_by_id(k_foreign, 3, 3u)->flags);
    CHECK(memory_source_find_by_token(k_host, 3, "drive8") == NULL);
    CHECK(memory_source_find_by_token(k_foreign, 3, "aux") == NULL);

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
