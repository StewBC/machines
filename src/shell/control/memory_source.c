#include "memory_source.h"

#include <string.h>

const memory_source *memory_source_find_by_id(
    const memory_source *table,
    size_t count,
    uint32_t id)
{
    size_t i;

    if (table == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        if (table[i].id == id) {
            return &table[i];
        }
    }
    return NULL;
}

const memory_source *memory_source_find_by_token(
    const memory_source *table,
    size_t count,
    const char *token)
{
    size_t i;

    if (table == NULL || token == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        if (table[i].token != NULL && strcmp(table[i].token, token) == 0) {
            return &table[i];
        }
    }
    return NULL;
}
