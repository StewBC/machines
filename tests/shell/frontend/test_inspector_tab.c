#include "inspector_tab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int g_record_calls;
static int g_record_want;
static int g_forensics_calls;
static int g_preview_calls;
static int g_preview_tick;
static int g_land_calls;
static int g_land_tick;

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", \
                    __FILE__, __LINE__, #expr); \
            failures++; \
        } \
    } while (0)

static void on_record(void *ctx, bool enabled)
{
    (void)ctx;
    g_record_calls++;
    g_record_want = enabled ? 1 : 0;
}

static void on_forensics(void *ctx)
{
    (void)ctx;
    g_forensics_calls++;
}

static void on_preview(void *ctx, int tick)
{
    (void)ctx;
    g_preview_calls++;
    g_preview_tick = tick;
}

static void on_land(void *ctx, int tick)
{
    (void)ctx;
    g_land_calls++;
    g_land_tick = tick;
}

int main(void)
{
    inspector_tab_view view;
    inspector_tab_state state;
    inspector_tab_ops ops;

    memset(&view, 0, sizeof(view));
    memset(&state, 0, sizeof(state));
    memset(&ops, 0, sizeof(ops));
    ops.on_record = on_record;
    ops.on_open_forensics = on_forensics;
    ops.on_preview_tick = on_preview;
    ops.on_land_tick = on_land;

    view.record_on = false;
    view.record_locked = true;
    CHECK(!inspector_tab_request_record(&view, &ops, true));
    CHECK(g_record_calls == 0);

    view.record_locked = false;
    CHECK(inspector_tab_request_record(&view, &ops, true));
    CHECK(g_record_calls == 1);
    CHECK(g_record_want == 1);
    view.record_on = true;
    CHECK(!inspector_tab_request_record(&view, &ops, true));
    CHECK(g_record_calls == 1);

    inspector_tab_request_forensics(&ops);
    CHECK(g_forensics_calls == 1);

    view.slider = 0;
    view.slider_max = 0;
    inspector_tab_process_slider(&state, &view, &ops, 4, true);
    CHECK(!state.thumb_down);
    CHECK(g_preview_calls == 0);
    CHECK(g_land_calls == 0);

    view.slider_max = 1000;
    inspector_tab_process_slider(&state, &view, &ops, 250, true);
    CHECK(state.thumb_down);
    CHECK(state.slider == 250);
    CHECK(g_preview_calls == 1);
    CHECK(g_preview_tick == 250);
    CHECK(g_land_calls == 0);

    inspector_tab_process_slider(&state, &view, &ops, 400, true);
    CHECK(state.thumb_down);
    CHECK(g_preview_calls == 2);
    CHECK(g_land_calls == 0);

    inspector_tab_process_slider(&state, &view, &ops, 400, false);
    CHECK(!state.thumb_down);
    CHECK(g_land_calls == 1);
    CHECK(g_land_tick == 400);
    CHECK(g_preview_calls == 2);

    view.slider = 12;
    inspector_tab_sync_slider(&state, &view);
    CHECK(state.slider == 12);

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
