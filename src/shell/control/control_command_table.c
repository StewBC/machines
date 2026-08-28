#include "control_command_table.h"

#include <string.h>

const control_verb *control_verb_lookup(
    const control_verb *table,
    size_t count,
    const char *name)
{
    size_t i;

    if (table == NULL || name == NULL || name[0] == '\0') {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        if (table[i].name != NULL && strcmp(table[i].name, name) == 0) {
            return &table[i];
        }
    }
    return NULL;
}

static int token_already_emitted(
    const char *buf,
    size_t used,
    const char *token,
    size_t token_len)
{
    size_t i = 0;

    while (i < used) {
        size_t n = 0;
        while (i + n < used && buf[i + n] != ' ') {
            n++;
        }
        if (n == token_len && memcmp(buf + i, token, token_len) == 0) {
            return 1;
        }
        i += n;
        if (i < used && buf[i] == ' ') {
            i++;
        }
    }
    return 0;
}

static size_t append_token(char *out, size_t out_size, size_t used, const char *token)
{
    size_t token_len;
    size_t need;

    if (out == NULL || out_size == 0u || token == NULL || token[0] == '\0') {
        return used;
    }
    token_len = strlen(token);
    if (token_already_emitted(out, used, token, token_len)) {
        return used;
    }
    need = token_len + (used > 0u ? 1u : 0u);
    if (used + need >= out_size) {
        return used;
    }
    if (used > 0u) {
        out[used++] = ' ';
    }
    memcpy(out + used, token, token_len);
    used += token_len;
    out[used] = '\0';
    return used;
}

static size_t append_token_list(
    char *out,
    size_t out_size,
    size_t used,
    const char *list)
{
    const char *p;

    if (list == NULL) {
        return used;
    }
    p = list;
    while (*p != '\0') {
        const char *start;
        char token[64];
        size_t n;

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        n = (size_t)(p - start);
        if (n == 0u || n >= sizeof(token)) {
            continue;
        }
        memcpy(token, start, n);
        token[n] = '\0';
        used = append_token(out, out_size, used, token);
    }
    return used;
}

size_t control_verb_format_capabilities(
    const control_verb *table,
    size_t count,
    char *out,
    size_t out_size)
{
    size_t i;
    size_t used = 0;

    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (table == NULL) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        used = append_token(out, out_size, used, table[i].capability);
        used = append_token_list(out, out_size, used, table[i].extra_capabilities);
    }
    return used;
}

control_verb_parse_status control_verb_split_and_lookup(
    const char *line,
    const control_verb *table,
    size_t count,
    control_framing_line *framing,
    const control_verb **out_verb)
{
    control_framing_split_status split;
    const control_verb *verb;

    if (out_verb != NULL) {
        *out_verb = NULL;
    }
    if (framing == NULL) {
        return CONTROL_VERB_PARSE_EMPTY;
    }
    split = control_framing_split_line(line, framing);
    if (split == CONTROL_FRAMING_SPLIT_EMPTY) {
        return CONTROL_VERB_PARSE_EMPTY;
    }
    if (split == CONTROL_FRAMING_SPLIT_BAD_ID) {
        return CONTROL_VERB_PARSE_BAD_ID;
    }
    if (split == CONTROL_FRAMING_SPLIT_MISSING_VERB) {
        return CONTROL_VERB_PARSE_MISSING_VERB;
    }
    verb = control_verb_lookup(table, count, framing->verb);
    if (verb == NULL) {
        return CONTROL_VERB_PARSE_UNKNOWN;
    }
    if (out_verb != NULL) {
        *out_verb = verb;
    }
    return CONTROL_VERB_PARSE_OK;
}
