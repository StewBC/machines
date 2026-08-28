#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    MEMSRC_HIGHBIT_ASCII = 1u << 0,
    MEMSRC_WRITABLE = 1u << 1,
    MEMSRC_FOREIGN_BUS = 1u << 2
};

typedef struct memory_source {
    uint32_t id;
    const char *label;
    const char *token;
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t flags;
} memory_source;

const memory_source *memory_source_find_by_id(
    const memory_source *table,
    size_t count,
    uint32_t id);

const memory_source *memory_source_find_by_token(
    const memory_source *table,
    size_t count,
    const char *token);
