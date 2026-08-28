#pragma once

#include "control_framing.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Shared control verb row. Leftover binaries supply tables.
 * name NULL = advertise-only (sessions / state-changed).
 * capability NULL = unadvertised (aliases).
 */
typedef struct control_verb {
    const char *name;
    const char *capability;
    const char *extra_capabilities;
    bool (*parse)(
        const char *rest,
        void *args_out,
        uint32_t request_id,
        control_response *err);
} control_verb;

typedef enum control_verb_parse_status {
    CONTROL_VERB_PARSE_OK = 0,
    CONTROL_VERB_PARSE_EMPTY,
    CONTROL_VERB_PARSE_BAD_ID,
    CONTROL_VERB_PARSE_MISSING_VERB,
    CONTROL_VERB_PARSE_UNKNOWN,
    CONTROL_VERB_PARSE_ARGS
} control_verb_parse_status;

const control_verb *control_verb_lookup(
    const control_verb *table,
    size_t count,
    const char *name);

size_t control_verb_format_capabilities(
    const control_verb *table,
    size_t count,
    char *out,
    size_t out_size);

control_verb_parse_status control_verb_split_and_lookup(
    const char *line,
    const control_verb *table,
    size_t count,
    control_framing_line *framing,
    const control_verb **out_verb);
