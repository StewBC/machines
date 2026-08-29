#include "control_verbs.h"

#include "runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define A2M_STAT_ISDIR(mode) (((mode) & _S_IFDIR) != 0)
#else
#define A2M_STAT_ISDIR(mode) S_ISDIR(mode)
#endif

typedef struct apple_control_verb {
    control_verb verb;
    control_command_type type;
} apple_control_verb;

static const char *skip_ws(const char *s)
{
    while (s != NULL && (*s == ' ' || *s == '\t')) {
        s++;
    }
    return s;
}

static bool parse_number(const char *s, char **end, unsigned long *out)
{
    const char *p = s;
    int base = 0;

    if (s == NULL || out == NULL) {
        return false;
    }
    if (*p == '$') {
        p++;
        base = 16;
    }
    *out = strtoul(p, end, base);
    if (end != NULL && *end == p) {
        return false;
    }
    return true;
}

static bool parse_u32(const char *s, char **end, uint32_t *out)
{
    unsigned long v;
    if (!parse_number(s, end, &v)) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static bool parse_u16_addr(const char *s, char **end, uint16_t *out)
{
    unsigned long v;
    if (!parse_number(s, end, &v)) {
        return false;
    }
    if (v > 0xFFFFul) {
        return false;
    }
    *out = (uint16_t)v;
    return true;
}

static void fail_args(control_response *err, uint32_t id, const char *msg)
{
    if (err != NULL) {
        control_protocol_format_error(err, id, "bad-args", msg, false);
    }
}

static bool parse_empty(
    const char *rest,
    void *args_out,
    uint32_t request_id,
    control_response *err)
{
    (void)rest;
    (void)args_out;
    (void)request_id;
    (void)err;
    return true;
}

static const memory_source k_apple_sources[] = {
    { 0u, "Map", "map", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 1u, "Main", "main", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 2u, "ROM", "rom", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII },
    { 3u, "Aux", "aux", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 4u, "LC1", "lc1", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE },
    { 5u, "LC2", "lc2", 0u, 0x10000u, MEMSRC_HIGHBIT_ASCII | MEMSRC_WRITABLE }
};

const memory_source *apple2_memory_sources(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(k_apple_sources) / sizeof(k_apple_sources[0]);
    }
    return k_apple_sources;
}

const char *control_protocol_memory_mode_name(uint32_t source_id)
{
    size_t n = 0;
    const memory_source *src =
        memory_source_find_by_id(apple2_memory_sources(&n), n, source_id);
    return (src != NULL && src->token != NULL) ? src->token : "map";
}

static bool parse_memory(
    const char *rest,
    void *args_out,
    uint32_t id,
    control_response *err,
    bool is_set)
{
    control_verb_args *args = args_out;
    char *end = NULL;
    char *cursor = (char *)skip_ws(rest);
    uint32_t length = 0;
    size_t n = 0;

    args->memory.source_id = k_apple_sources[0].id;
    if (!parse_u16_addr(cursor, &end, &args->memory.address)) {
        fail_args(err, id, "address");
        return false;
    }
    cursor = (char *)skip_ws(end);
    if (!parse_u32(cursor, &end, &length) || length == 0u || length > 65536u) {
        fail_args(err, id, "length");
        return false;
    }
    args->memory.length = length;
    cursor = (char *)skip_ws(end);
    if (cursor[0] != '\0') {
        char mode_tok[16];
        size_t mi = 0;
        const memory_source *src;
        while (cursor[mi] != '\0' && !isspace((unsigned char)cursor[mi]) &&
               mi + 1u < sizeof(mode_tok)) {
            mode_tok[mi] = (char)tolower((unsigned char)cursor[mi]);
            mi++;
        }
        mode_tok[mi] = '\0';
        src = memory_source_find_by_token(apple2_memory_sources(&n), n, mode_tok);
        if (src == NULL) {
            fail_args(err, id, "mode");
            return false;
        }
        args->memory.source_id = src->id;
    }
    if (is_set && args->memory.length > 1024u) {
        fail_args(err, id, "set-memory length max 1024");
        return false;
    }
    return true;
}

static bool parse_get_memory(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    return parse_memory(rest, args_out, id, err, false);
}

static bool parse_set_memory(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    return parse_memory(rest, args_out, id, err, true);
}

static bool parse_set_reg(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    size_t i = 0;
    uint32_t v = 0;

    while (cursor[i] != '\0' && !isspace((unsigned char)cursor[i]) &&
           i + 1u < sizeof(args->set_reg.name)) {
        args->set_reg.name[i] = (char)tolower((unsigned char)cursor[i]);
        i++;
    }
    args->set_reg.name[i] = '\0';
    cursor = (char *)skip_ws(cursor + i);
    if (args->set_reg.name[0] == '\0' || !parse_u32(cursor, &end, &v)) {
        fail_args(err, id, "set-reg");
        return false;
    }
    args->set_reg.value = (uint16_t)v;
    return true;
}

static bool parse_set_turbo(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char token[64];
    size_t ti = 0;
    uint32_t milli = 0;
    const char *s = skip_ws(rest);

    while (*s != '\0' && !isspace((unsigned char)*s) && ti + 1u < sizeof(token)) {
        token[ti++] = *s++;
    }
    token[ti] = '\0';
    if (ti == 0u || !runtime_turbo_parse_token(token, &milli)) {
        fail_args(err, id, "turbo MHz|max|-1");
        return false;
    }
    args->turbo.milli_mhz = milli;
    return true;
}

static bool parse_break_exec(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *end = NULL;
    if (!parse_u16_addr(skip_ws(rest), &end, &args->brk.address)) {
        fail_args(err, id, "address");
        return false;
    }
    return true;
}

static bool parse_break_clear(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    uint32_t bid = 0;

    if (cursor[0] == '\0' || strcmp(cursor, "all") == 0) {
        args->brk.id = 0;
        return true;
    }
    if (!parse_u32(cursor, &end, &bid)) {
        fail_args(err, id, "id");
        return false;
    }
    args->brk.id = bid;
    return true;
}

static bool parse_break_enable(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    uint32_t bid = 0;
    uint32_t en = 0;

    if (!parse_u32(cursor, &end, &bid)) {
        fail_args(err, id, "id");
        return false;
    }
    cursor = (char *)skip_ws(end);
    if (!parse_u32(cursor, &end, &en) || en > 1u) {
        fail_args(err, id, "enable 0|1");
        return false;
    }
    args->brk.id = bid;
    args->brk.enable = (uint8_t)en;
    return true;
}

static bool parse_break_create(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    const char *cursor = skip_ws(rest);
    if (cursor[0] == '\0') {
        fail_args(err, id, "expected breakpoint definition");
        return false;
    }
    strncpy(args->brk.text, cursor, sizeof(args->brk.text) - 1u);
    args->brk.text[sizeof(args->brk.text) - 1u] = '\0';
    return true;
}

static bool parse_break_update(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    uint32_t bid = 0;

    if (!parse_u32(cursor, &end, &bid)) {
        fail_args(err, id, "id");
        return false;
    }
    cursor = (char *)skip_ws(end);
    if (cursor[0] == '\0') {
        fail_args(err, id, "expected breakpoint definition");
        return false;
    }
    args->brk.id = bid;
    strncpy(args->brk.text, cursor, sizeof(args->brk.text) - 1u);
    args->brk.text[sizeof(args->brk.text) - 1u] = '\0';
    return true;
}

static bool parse_wait_paused_running(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    uint32_t t = 0;

    args->wait.timeout_ms = 2000u;
    if (cursor[0] != '\0') {
        if (!parse_u32(cursor, &end, &t) || t < 1u || t > 600000u) {
            fail_args(err, id, "timeout");
            return false;
        }
        args->wait.timeout_ms = t;
    }
    return true;
}

static bool parse_wait_frame(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    uint32_t delta = 0;
    uint32_t t = 0;

    args->wait.timeout_ms = 2000u;
    args->wait.frame_delta = 1u;
    if (!parse_u32(cursor, &end, &delta) || delta < 1u) {
        fail_args(err, id, "frame-delta");
        return false;
    }
    args->wait.frame_delta = delta;
    cursor = (char *)skip_ws(end);
    if (cursor[0] != '\0') {
        if (!parse_u32(cursor, &end, &t) || t < 1u || t > 600000u) {
            fail_args(err, id, "timeout");
            return false;
        }
        args->wait.timeout_ms = t;
    }
    return true;
}

static bool parse_wait_event(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    size_t i = 0;
    char *end = NULL;
    uint32_t t = 0;

    args->wait.timeout_ms = 2000u;
    while (cursor[i] != '\0' && !isspace((unsigned char)cursor[i]) &&
           i + 1u < sizeof(args->wait.event_name)) {
        args->wait.event_name[i] = cursor[i];
        i++;
    }
    args->wait.event_name[i] = '\0';
    if (args->wait.event_name[0] == '\0') {
        fail_args(err, id, "event-name");
        return false;
    }
    cursor = (char *)skip_ws(cursor + i);
    if (cursor[0] != '\0') {
        if (!parse_u32(cursor, &end, &t) || t < 1u || t > 600000u) {
            fail_args(err, id, "timeout");
            return false;
        }
        args->wait.timeout_ms = t;
    }
    return true;
}

static bool parse_on_off(const char *cursor, bool *out)
{
    if (strcmp(cursor, "on") == 0 || strcmp(cursor, "1") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(cursor, "off") == 0 || strcmp(cursor, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool parse_frame_ring_record(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    if (!parse_on_off(skip_ws(rest), &args->frame_ring.record_enabled)) {
        fail_args(err, id, "on|off");
        return false;
    }
    return true;
}

static bool parse_history_record(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    if (!parse_on_off(skip_ws(rest), &args->history.record_enabled)) {
        fail_args(err, id, "on|off");
        return false;
    }
    return true;
}

static bool parse_history_find(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    (void)id;
    (void)err;
    strncpy(args->history.find_text, skip_ws(rest), sizeof(args->history.find_text) - 1u);
    args->history.find_text[sizeof(args->history.find_text) - 1u] = '\0';
    return true;
}

static bool parse_history_next(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    unsigned long cursor_v = 0;
    uint32_t limit = 64;

    args->history.limit = 64u;
    if (!parse_number(cursor, &end, &cursor_v) || cursor_v == 0ul) {
        fail_args(err, id, "cursor");
        return false;
    }
    args->history.cursor = (uint64_t)cursor_v;
    cursor = (char *)skip_ws(end);
    if (cursor[0] != '\0') {
        if (strncmp(cursor, "limit=", 6) == 0) {
            if (!parse_u32(cursor + 6, &end, &limit) || limit < 1u || limit > 256u) {
                fail_args(err, id, "limit");
                return false;
            }
        } else if (!parse_u32(cursor, &end, &limit) || limit < 1u || limit > 256u) {
            fail_args(err, id, "limit");
            return false;
        }
        args->history.limit = (uint16_t)limit;
    }
    return true;
}

static bool parse_history_read(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    unsigned long id_v = 0;

    args->history.before = 32u;
    args->history.after = 8u;
    if (!parse_number(cursor, &end, &id_v) || id_v == 0ul) {
        fail_args(err, id, "id");
        return false;
    }
    args->history.id = (uint64_t)id_v;
    cursor = (char *)skip_ws(end);
    while (cursor[0] != '\0') {
        char key[16];
        size_t ki = 0;
        char *eq = strchr(cursor, '=');
        unsigned long v = 0;
        if (eq == NULL) {
            fail_args(err, id, "key=value");
            return false;
        }
        while (cursor[ki] != '=' && ki + 1u < sizeof(key)) {
            key[ki] = cursor[ki];
            ki++;
        }
        key[ki] = '\0';
        if (!parse_number(eq + 1, &end, &v)) {
            fail_args(err, id, key);
            return false;
        }
        if (strcmp(key, "epoch") == 0) {
            args->history.epoch = (uint64_t)v;
        } else if (strcmp(key, "before") == 0) {
            if (v > 256ul) {
                fail_args(err, id, "before");
                return false;
            }
            args->history.before = (uint16_t)v;
        } else if (strcmp(key, "after") == 0) {
            if (v > 256ul) {
                fail_args(err, id, "after");
                return false;
            }
            args->history.after = (uint16_t)v;
        } else {
            fail_args(err, id, "unknown key");
            return false;
        }
        cursor = (char *)skip_ws(end);
    }
    return true;
}

static bool parse_history_close(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    char *end = NULL;
    unsigned long cursor_v = 0;

    if (cursor[0] == '\0') {
        args->history.cursor = 0;
        return true;
    }
    if (!parse_number(cursor, &end, &cursor_v)) {
        fail_args(err, id, "cursor");
        return false;
    }
    args->history.cursor = (uint64_t)cursor_v;
    return true;
}

static bool parse_get_frame_at(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    const char *cursor = skip_ws(rest);
    char key[16];
    size_t ki = 0;
    char *eq;
    char *end = NULL;
    unsigned long v = 0;

    if (cursor[0] == '\0') {
        fail_args(err, id, "frame=<n>|cycle=<n>");
        return false;
    }
    while (cursor[ki] != '\0' && cursor[ki] != '=' && ki + 1u < sizeof(key)) {
        key[ki] = cursor[ki];
        ki++;
    }
    key[ki] = '\0';
    eq = strchr(cursor, '=');
    if (eq == NULL || !parse_number(eq + 1, &end, &v)) {
        fail_args(err, id, "frame=<n>|cycle=<n>");
        return false;
    }
    args->frame_ring.target = (uint64_t)v;
    if (strcmp(key, "frame") == 0) {
        args->frame_ring.by_cycle = false;
    } else if (strcmp(key, "cycle") == 0) {
        args->frame_ring.by_cycle = true;
    } else {
        fail_args(err, id, "frame=<n>|cycle=<n>");
        return false;
    }
    return true;
}

static bool parse_path(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    const char *cursor = skip_ws(rest);
    if (cursor[0] == '\0') {
        fail_args(err, id, "path");
        return false;
    }
    strncpy(args->path.path, cursor, sizeof(args->path.path) - 1u);
    args->path.path[sizeof(args->path.path) - 1u] = '\0';
    return true;
}

static int parse_assemble_option(char **cursor, control_args_assemble *as)
{
    char *start;
    char *end;
    char *value_end;
    size_t length;

    if (cursor == NULL || *cursor == NULL || as == NULL) {
        return 0;
    }
    start = (char *)skip_ws(*cursor);
    if (*start == '\0') {
        return 0;
    }
    end = start;
    while (*end != '\0' && !isspace((unsigned char)*end)) {
        end++;
    }
    length = (size_t)(end - start);
    if (length > 8 && strncmp(start, "address=", 8) == 0) {
        if (!parse_u16_addr(start + 8, &value_end, &as->address) || value_end != end) {
            return -1;
        }
    } else if (length > 12 && strncmp(start, "run-address=", 12) == 0) {
        if (!parse_u16_addr(start + 12, &value_end, &as->run_address) || value_end != end) {
            return -1;
        }
        as->has_run_address = true;
    } else if (length > 9 && strncmp(start, "auto-run=", 9) == 0) {
        if (length == 10 && start[9] == '0') {
            as->auto_run = false;
        } else if (length == 10 && start[9] == '1') {
            as->auto_run = true;
        } else {
            return -1;
        }
    } else if (length > 11 && strncmp(start, "mli-launch=", 11) == 0) {
        if (length == 12 && start[11] == '0') {
            as->mli_launch = false;
        } else if (length == 12 && start[11] == '1') {
            as->mli_launch = true;
        } else {
            return -1;
        }
    } else if (length > 6 && strncmp(start, "reset=", 6) == 0) {
        if (length == 7 && start[6] == '0') {
            as->reset_first = false;
        } else if (length == 7 && start[6] == '1') {
            as->reset_first = true;
        } else {
            return -1;
        }
    } else if (length > 21 && strncmp(start, "auto-adjust-segments=", 21) == 0) {
        if (length == 22 && start[21] == '0') {
            as->auto_adjust_segments = false;
        } else if (length == 22 && start[21] == '1') {
            as->auto_adjust_segments = true;
        } else {
            return -1;
        }
    } else {
        return 0;
    }
    *cursor = end;
    return 1;
}

static bool parse_assemble(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);

    args->assemble.address = 0x8000u;
    args->assemble.run_address = 0x8000u;
    args->assemble.has_run_address = false;
    args->assemble.auto_run = false;
    args->assemble.mli_launch = false;
    args->assemble.reset_first = true;
    args->assemble.auto_adjust_segments = false;
    for (;;) {
        int opt = parse_assemble_option(&cursor, &args->assemble);
        if (opt < 0) {
            fail_args(err, id, "invalid assembler option");
            return false;
        }
        if (opt == 0) {
            break;
        }
    }
    cursor = (char *)skip_ws(cursor);
    if (!args->assemble.has_run_address) {
        args->assemble.run_address = args->assemble.address;
    }
    if (args->assemble.mli_launch && args->assemble.reset_first) {
        fail_args(err, id, "mli-launch and reset are mutually exclusive");
        return false;
    }
    if (args->assemble.mli_launch) {
        args->assemble.auto_run = true;
        args->assemble.reset_first = false;
    }
    if (cursor[0] == '\0') {
        fail_args(err, id, "expected source path");
        return false;
    }
    strncpy(args->assemble.path, cursor, sizeof(args->assemble.path) - 1u);
    args->assemble.path[sizeof(args->assemble.path) - 1u] = '\0';
    return true;
}

static bool parse_find_symbol(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    const char *cursor = skip_ws(rest);
    size_t i = 0;

    while (cursor[i] != '\0' && !isspace((unsigned char)cursor[i]) &&
           i + 1u < sizeof(args->find_symbol.name)) {
        args->find_symbol.name[i] = cursor[i];
        i++;
    }
    args->find_symbol.name[i] = '\0';
    if (args->find_symbol.name[0] == '\0') {
        fail_args(err, id, "expected symbol name");
        return false;
    }
    cursor = skip_ws(cursor + i);
    if (cursor[0] != '\0') {
        fail_args(err, id, "unexpected arguments");
        return false;
    }
    return true;
}

static bool parse_key(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *end = NULL;
    uint32_t k = 0;
    if (!parse_u32(skip_ws(rest), &end, &k)) {
        fail_args(err, id, "key");
        return false;
    }
    args->key.value = (uint8_t)k;
    return true;
}

static bool path_ends_with_ci(const char *path, const char *ext)
{
    size_t path_len;
    size_t ext_len;
    size_t i;

    if (path == NULL || ext == NULL) {
        return false;
    }
    path_len = strlen(path);
    ext_len = strlen(ext);
    if (ext_len == 0u || path_len < ext_len + 1u) {
        return false;
    }
    if (path[path_len - ext_len - 1u] != '.') {
        return false;
    }
    for (i = 0; i < ext_len; ++i) {
        char a = path[path_len - ext_len + i];
        char b = ext[i];
        if (tolower((unsigned char)a) != tolower((unsigned char)b)) {
            return false;
        }
    }
    return true;
}

static bool parse_optional_media_kind(
    char **inout_cursor,
    uint8_t *out_kind,
    control_response *out_error,
    uint32_t id)
{
    char *cursor;
    char token[32];
    size_t i = 0;

    if (inout_cursor == NULL || out_kind == NULL) {
        return false;
    }
    cursor = (char *)skip_ws(*inout_cursor);
    if (strncmp(cursor, "kind=", 5) != 0) {
        *out_kind = (uint8_t)CONTROL_MEDIA_KIND_UNSPECIFIED;
        *inout_cursor = cursor;
        return true;
    }
    cursor += 5;
    while (cursor[i] != '\0' && cursor[i] != ' ' && cursor[i] != '\t' &&
           i + 1u < sizeof(token)) {
        token[i] = (char)tolower((unsigned char)cursor[i]);
        i++;
    }
    token[i] = '\0';
    if (strcmp(token, "diskii") == 0 || strcmp(token, "disk") == 0) {
        *out_kind = (uint8_t)CONTROL_MEDIA_KIND_DISKII;
    } else if (
        strcmp(token, "smartport") == 0 || strcmp(token, "sp") == 0 ||
        strcmp(token, "hd") == 0) {
        *out_kind = (uint8_t)CONTROL_MEDIA_KIND_SMARTPORT;
    } else {
        fail_args(out_error, id, "kind");
        return false;
    }
    *inout_cursor = (char *)skip_ws(cursor + i);
    return true;
}

static bool infer_media_kind_from_path(
    const char *path,
    uint8_t *out_kind,
    control_response *out_error,
    uint32_t id)
{
    struct stat st;

    if (path == NULL || path[0] == '\0' || out_kind == NULL) {
        fail_args(out_error, id, "path");
        return false;
    }
    if (stat(path, &st) == 0 && A2M_STAT_ISDIR(st.st_mode)) {
        *out_kind = (uint8_t)CONTROL_MEDIA_KIND_SMARTPORT;
        return true;
    }
    if (path_ends_with_ci(path, "nib") || path_ends_with_ci(path, "dsk") ||
        path_ends_with_ci(path, "do") || path_ends_with_ci(path, "woz")) {
        *out_kind = (uint8_t)CONTROL_MEDIA_KIND_DISKII;
        return true;
    }
    if (path_ends_with_ci(path, "hdv") || path_ends_with_ci(path, "2mg")) {
        *out_kind = (uint8_t)CONTROL_MEDIA_KIND_SMARTPORT;
        return true;
    }
    if (path_ends_with_ci(path, "po")) {
        fail_args(out_error, id, "kind= required for .po");
        return false;
    }
    fail_args(out_error, id, "media-kind");
    return false;
}

static bool parse_mount_slot_drive_path(
    char *cursor,
    control_args_media *media,
    control_response *out_error,
    uint32_t id)
{
    uint32_t a = 0;
    uint32_t b = 0;
    char *e1 = NULL;
    char *p2;
    char *e2 = NULL;
    char *p3;

    media->slot = 0u;
    media->drive = 0u;
    if (parse_u32(cursor, &e1, &a) && e1 != cursor &&
        (*e1 == ' ' || *e1 == '\t')) {
        p2 = (char *)skip_ws(e1);
        if (parse_u32(p2, &e2, &b) && e2 != p2 &&
            (*e2 == ' ' || *e2 == '\t')) {
            if (a < 1u || a > 7u || b > 1u) {
                fail_args(out_error, id, "slot/drive");
                return false;
            }
            media->slot = (uint8_t)a;
            media->drive = (uint8_t)b;
            cursor = (char *)skip_ws(e2);
        } else {
            if (a > 1u) {
                fail_args(out_error, id, "drive");
                return false;
            }
            media->drive = (uint8_t)a;
            cursor = p2;
        }
    }
    p3 = (char *)skip_ws(cursor);
    if (p3[0] == '\0') {
        fail_args(out_error, id, "path");
        return false;
    }
    strncpy(media->path, p3, sizeof(media->path) - 1u);
    media->path[sizeof(media->path) - 1u] = '\0';
    return true;
}

static bool parse_unmount_slot_drive(
    char *cursor,
    control_args_media *media,
    control_response *out_error,
    uint32_t id)
{
    uint32_t a = 0;
    uint32_t b = 0;
    char *e1 = NULL;
    char *p2;
    char *e2 = NULL;

    media->slot = 0u;
    media->drive = 0u;
    cursor = (char *)skip_ws(cursor);
    if (cursor[0] == '\0') {
        return true;
    }
    if (!parse_u32(cursor, &e1, &a) || e1 == cursor) {
        fail_args(out_error, id, "drive");
        return false;
    }
    p2 = (char *)skip_ws(e1);
    if (*p2 == '\0') {
        if (a > 1u) {
            fail_args(out_error, id, "drive");
            return false;
        }
        media->drive = (uint8_t)a;
        return true;
    }
    if (!parse_u32(p2, &e2, &b) || e2 == p2 || *skip_ws(e2) != '\0') {
        fail_args(out_error, id, "slot/drive");
        return false;
    }
    if (a < 1u || a > 7u || b > 1u) {
        fail_args(out_error, id, "slot/drive");
        return false;
    }
    media->slot = (uint8_t)a;
    media->drive = (uint8_t)b;
    return true;
}

static bool parse_mount_disk(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    args->media.kind = (uint8_t)CONTROL_MEDIA_KIND_DISKII;
    return parse_mount_slot_drive_path(cursor, &args->media, err, id);
}

static bool parse_mount(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    if (!parse_optional_media_kind(&cursor, &args->media.kind, err, id)) {
        return false;
    }
    if (!parse_mount_slot_drive_path(cursor, &args->media, err, id)) {
        return false;
    }
    if (args->media.kind == (uint8_t)CONTROL_MEDIA_KIND_UNSPECIFIED) {
        if (!infer_media_kind_from_path(args->media.path, &args->media.kind, err, id)) {
            return false;
        }
    }
    return true;
}

static bool parse_unmount(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    if (!parse_optional_media_kind(&cursor, &args->media.kind, err, id)) {
        return false;
    }
    return parse_unmount_slot_drive(cursor, &args->media, err, id);
}

static bool parse_select_disk(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    char *e1 = NULL;
    char *p2;
    char *e2 = NULL;
    char *p3;
    char *e3 = NULL;

    args->media.slot = 0u;
    args->media.drive = 0u;
    if (!parse_u32(cursor, &e1, &a) || e1 == cursor) {
        fail_args(err, id, "index");
        return false;
    }
    p2 = (char *)skip_ws(e1);
    if (*p2 == '\0') {
        args->media.disk_index = a;
        return true;
    }
    if (!parse_u32(p2, &e2, &b) || e2 == p2) {
        fail_args(err, id, "index");
        return false;
    }
    p3 = (char *)skip_ws(e2);
    if (*p3 == '\0') {
        args->media.drive = (uint8_t)a;
        args->media.disk_index = b;
        return true;
    }
    if (!parse_u32(p3, &e3, &c) || e3 == p3 || *skip_ws(e3) != '\0') {
        fail_args(err, id, "index");
        return false;
    }
    args->media.slot = (uint8_t)a;
    args->media.drive = (uint8_t)b;
    args->media.disk_index = c;
    return true;
}

static bool parse_set_disk_writable(
    const char *rest, void *args_out, uint32_t id, control_response *err)
{
    control_verb_args *args = args_out;
    char *cursor = (char *)skip_ws(rest);
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    char *e1 = NULL;
    char *p2;
    char *e2 = NULL;
    char *p3;
    char *e3 = NULL;

    args->media.slot = 0u;
    args->media.drive = 0u;
    if (!parse_u32(cursor, &e1, &a) || e1 == cursor) {
        fail_args(err, id, "writable");
        return false;
    }
    p2 = (char *)skip_ws(e1);
    if (*p2 == '\0') {
        if (a > 1u) {
            fail_args(err, id, "writable");
            return false;
        }
        args->media.writable = (uint8_t)a;
        return true;
    }
    if (!parse_u32(p2, &e2, &b) || e2 == p2) {
        fail_args(err, id, "writable");
        return false;
    }
    p3 = (char *)skip_ws(e2);
    if (*p3 == '\0') {
        if (b > 1u) {
            fail_args(err, id, "writable");
            return false;
        }
        args->media.drive = (uint8_t)a;
        args->media.writable = (uint8_t)b;
        return true;
    }
    if (!parse_u32(p3, &e3, &c) || e3 == p3 || *skip_ws(e3) != '\0' || c > 1u) {
        fail_args(err, id, "writable");
        return false;
    }
    args->media.slot = (uint8_t)a;
    args->media.drive = (uint8_t)b;
    args->media.writable = (uint8_t)c;
    return true;
}

static const apple_control_verb k_apple_verbs[] = {
    { { "hello", "connection", NULL, parse_empty }, CONTROL_COMMAND_HELLO },
    { { "version", "introspection", NULL, parse_empty }, CONTROL_COMMAND_VERSION },
    { { "capabilities", "introspection", NULL, parse_empty }, CONTROL_COMMAND_CAPABILITIES },
    { { "ping", "connection", NULL, parse_empty }, CONTROL_COMMAND_PING },
    { { "quit-client", "connection", NULL, parse_empty }, CONTROL_COMMAND_QUIT_CLIENT },
    { { "reset", "execution", NULL, parse_empty }, CONTROL_COMMAND_RESET },
    { { "run", "execution", NULL, parse_empty }, CONTROL_COMMAND_RUN },
    { { "pause", "execution", NULL, parse_empty }, CONTROL_COMMAND_PAUSE },
    { { "get-state", "state", NULL, parse_empty }, CONTROL_COMMAND_GET_STATE },
    { { "get-cpu", "introspection", NULL, parse_empty }, CONTROL_COMMAND_GET_CPU },
    { { "get-softswitches", "softswitches", NULL, parse_empty }, CONTROL_COMMAND_GET_SOFTSWITCHES },
    { { "step-cycle", "step", NULL, parse_empty }, CONTROL_COMMAND_STEP_CYCLE },
    { { "step-instruction", "step", NULL, parse_empty }, CONTROL_COMMAND_STEP_INSTRUCTION },
    { { "step-over", "step", NULL, parse_empty }, CONTROL_COMMAND_STEP_OVER },
    { { "step-out", "step", NULL, parse_empty }, CONTROL_COMMAND_STEP_OUT },
    { { "set-turbo", "turbo", NULL, parse_set_turbo }, CONTROL_COMMAND_SET_TURBO },
    { { "get-frame", "frame", NULL, parse_empty }, CONTROL_COMMAND_GET_FRAME },
    { { "frame-ring-info", "frame-ring", NULL, parse_empty }, CONTROL_COMMAND_FRAME_RING_INFO },
    { { "frame-ring-record", "frame-ring", NULL, parse_frame_ring_record }, CONTROL_COMMAND_FRAME_RING_RECORD },
    { { "frame-ring-clear", "frame-ring", NULL, parse_empty }, CONTROL_COMMAND_FRAME_RING_CLEAR },
    { { "get-frame-at", "frame-ring", NULL, parse_get_frame_at }, CONTROL_COMMAND_GET_FRAME_AT },
    { { "get-memory", "memory", NULL, parse_get_memory }, CONTROL_COMMAND_GET_MEMORY },
    { { "set-memory", "memory", NULL, parse_set_memory }, CONTROL_COMMAND_SET_MEMORY },
    { { "set-reg", NULL, NULL, parse_set_reg }, CONTROL_COMMAND_SET_REG },
    { { "break-exec", "breakpoints", NULL, parse_break_exec }, CONTROL_COMMAND_BREAK_EXEC },
    { { "break-clear", "breakpoints", NULL, parse_break_clear }, CONTROL_COMMAND_BREAK_CLEAR },
    { { "break-clear-all", "breakpoints", NULL, parse_empty }, CONTROL_COMMAND_BREAK_CLEAR_ALL },
    { { "break-enable", "breakpoints", NULL, parse_break_enable }, CONTROL_COMMAND_BREAK_ENABLE },
    { { "break-list", "breakpoints", NULL, parse_empty }, CONTROL_COMMAND_BREAK_LIST },
    { { "get-breakpoints", NULL, NULL, parse_empty }, CONTROL_COMMAND_BREAK_LIST },
    { { "break-create", "breakpoints", NULL, parse_break_create }, CONTROL_COMMAND_BREAK_CREATE },
    { { "break-update", "breakpoints", NULL, parse_break_update }, CONTROL_COMMAND_BREAK_UPDATE },
    { { "rearm-oneshots", "breakpoints", NULL, parse_empty }, CONTROL_COMMAND_REARM_ONESHOTS },
    { { "wait-paused", "wait", NULL, parse_wait_paused_running }, CONTROL_COMMAND_WAIT_PAUSED },
    { { "wait-running", "wait", NULL, parse_wait_paused_running }, CONTROL_COMMAND_WAIT_RUNNING },
    { { "wait-frame", "wait", NULL, parse_wait_frame }, CONTROL_COMMAND_WAIT_FRAME },
    { { "wait-event", "wait", NULL, parse_wait_event }, CONTROL_COMMAND_WAIT_EVENT },
    { { "key", "key", NULL, parse_key }, CONTROL_COMMAND_KEY },
    { { "mount", "disk", NULL, parse_mount }, CONTROL_COMMAND_MOUNT },
    { { "mount-disk", "disk", NULL, parse_mount_disk }, CONTROL_COMMAND_MOUNT_DISK },
    { { "unmount", "disk", NULL, parse_unmount }, CONTROL_COMMAND_UNMOUNT },
    { { "select-disk", "disk", NULL, parse_select_disk }, CONTROL_COMMAND_SELECT_DISK },
    { { "set-disk-writable", "disk", NULL, parse_set_disk_writable }, CONTROL_COMMAND_SET_DISK_WRITABLE },
    { { "save-state", "snapshot", NULL, parse_path }, CONTROL_COMMAND_SAVE_STATE },
    { { "load-state", "snapshot", NULL, parse_path }, CONTROL_COMMAND_LOAD_STATE },
    { { "history-info", "history", NULL, parse_empty }, CONTROL_COMMAND_HISTORY_INFO },
    { { "history-record", "history", NULL, parse_history_record }, CONTROL_COMMAND_HISTORY_RECORD },
    { { "history-clear", "history", NULL, parse_empty }, CONTROL_COMMAND_HISTORY_CLEAR },
    { { "history-find", "history", NULL, parse_history_find }, CONTROL_COMMAND_HISTORY_FIND },
    { { "history-next", "history", NULL, parse_history_next }, CONTROL_COMMAND_HISTORY_NEXT },
    { { "history-read", "history", NULL, parse_history_read }, CONTROL_COMMAND_HISTORY_READ },
    { { "history-close", "history", NULL, parse_history_close }, CONTROL_COMMAND_HISTORY_CLOSE },
    { { "assemble", "assemble", "mli-launch", parse_assemble }, CONTROL_COMMAND_ASSEMBLE },
    { { "find-symbol", "symbols", NULL, parse_find_symbol }, CONTROL_COMMAND_FIND_SYMBOL },
    { { NULL, "sessions", NULL, NULL }, CONTROL_COMMAND_NONE },
    { { NULL, "state-changed", NULL, NULL }, CONTROL_COMMAND_NONE },
    { { "leave-inspector", "inspector", NULL, parse_empty }, CONTROL_COMMAND_LEAVE_INSPECTOR },
    { { "enter-inspector", "inspector", NULL, parse_empty }, CONTROL_COMMAND_ENTER_INSPECTOR }
};

static const apple_control_verb *find_apple_verb(const char *name)
{
    size_t i;
    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < sizeof(k_apple_verbs) / sizeof(k_apple_verbs[0]); i++) {
        if (k_apple_verbs[i].verb.name != NULL &&
            strcmp(k_apple_verbs[i].verb.name, name) == 0) {
            return &k_apple_verbs[i];
        }
    }
    return NULL;
}

void apple2_control_format_capabilities(char *out, size_t out_size)
{
    control_verb packed[sizeof(k_apple_verbs) / sizeof(k_apple_verbs[0])];
    size_t i;
    for (i = 0; i < sizeof(k_apple_verbs) / sizeof(k_apple_verbs[0]); i++) {
        packed[i] = k_apple_verbs[i].verb;
    }
    control_verb_format_capabilities(
        packed, sizeof(packed) / sizeof(packed[0]), out, out_size);
}

bool apple2_control_parse_line(
    const char *line,
    control_request *out_request,
    control_response *out_error)
{
    char buf[CONTROL_LINE_MAX];
    control_framing_line framing;
    control_framing_split_status split;
    const apple_control_verb *row;
    size_t i;

    if (line == NULL || out_request == NULL) {
        return false;
    }
    memset(out_request, 0, sizeof(*out_request));
    strncpy(buf, line, sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';
    for (i = 0; buf[i] != '\0'; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') {
            buf[i] = '\0';
            break;
        }
    }

    split = control_framing_split_line(buf, &framing);
    if (split != CONTROL_FRAMING_SPLIT_OK) {
        if (out_error != NULL) {
            if (split == CONTROL_FRAMING_SPLIT_BAD_ID) {
                control_protocol_format_error(out_error, 0, "bad-id", "missing id", false);
            } else if (split == CONTROL_FRAMING_SPLIT_MISSING_VERB) {
                control_protocol_format_error(
                    out_error, framing.id, "bad-request", "missing command", false);
            } else {
                control_protocol_format_error(out_error, 0, "bad-request", "empty", false);
            }
        }
        return false;
    }

    row = find_apple_verb(framing.verb);
    if (row == NULL) {
        if (out_error != NULL) {
            control_protocol_format_error(
                out_error, framing.id, "unknown-command", framing.verb, false);
        }
        return false;
    }

    out_request->id = framing.id;
    out_request->type = row->type;
    out_request->verb = &row->verb;
    if (row->verb.parse != NULL &&
        !row->verb.parse(framing.rest, &out_request->args, framing.id, out_error)) {
        return false;
    }
    if (out_request->type == CONTROL_COMMAND_SET_MEMORY) {
        out_request->payload_size = out_request->args.memory.length;
    }
    return true;
}
