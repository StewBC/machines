#include "control_command_table.h"

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

static bool parse_fail_args(
    const char *rest,
    void *args_out,
    uint32_t request_id,
    control_response *err)
{
    (void)rest;
    (void)args_out;
    if (err != NULL) {
        control_protocol_format_error(err, request_id, "bad-args", "nope", false);
    }
    return false;
}

static const control_verb k_table[] = {
    { "hello", "connection", NULL, parse_empty },
    { "ping", "connection", NULL, parse_empty },
    { "assemble", "assemble", "mli-launch", parse_empty },
    { "alias-assemble", NULL, NULL, parse_empty },
    { NULL, "sessions", NULL, NULL },
    { NULL, "state-changed", NULL, NULL },
    { "leave-inspector", "inspector", NULL, parse_fail_args }
};

int main(void)
{
    char caps[256];
    control_framing_line framing;
    const control_verb *verb;
    control_verb_parse_status st;

    CHECK(control_verb_lookup(k_table, 7, "hello") == &k_table[0]);
    CHECK(control_verb_lookup(k_table, 7, "ping") == &k_table[1]);
    CHECK(control_verb_lookup(k_table, 7, "sessions") == NULL);
    CHECK(control_verb_lookup(k_table, 7, "foobar") == NULL);
    CHECK(control_verb_lookup(k_table, 7, "alias-assemble") == &k_table[3]);

    CHECK(control_verb_format_capabilities(k_table, 7, caps, sizeof(caps)) > 0);
    CHECK(strcmp(
              caps,
              "connection assemble mli-launch sessions state-changed inspector") ==
          0);

    st = control_verb_split_and_lookup(
        "10 foobar extra", k_table, 7, &framing, &verb);
    CHECK(st == CONTROL_VERB_PARSE_UNKNOWN);
    CHECK(verb == NULL);
    CHECK(framing.id == 10);
    CHECK(strcmp(framing.verb, "foobar") == 0);

    st = control_verb_split_and_lookup("1 hello", k_table, 7, &framing, &verb);
    CHECK(st == CONTROL_VERB_PARSE_OK);
    CHECK(verb == &k_table[0]);

    st = control_verb_split_and_lookup("", k_table, 7, &framing, &verb);
    CHECK(st == CONTROL_VERB_PARSE_EMPTY);

    st = control_verb_split_and_lookup("x ping", k_table, 7, &framing, &verb);
    CHECK(st == CONTROL_VERB_PARSE_BAD_ID);

    st = control_verb_split_and_lookup("3", k_table, 7, &framing, &verb);
    CHECK(st == CONTROL_VERB_PARSE_MISSING_VERB);

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
