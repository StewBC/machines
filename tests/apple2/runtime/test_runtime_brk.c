#include "runtime.h"
#include "runtime_client.h"
#include "runtime_event.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, int v)
{
    if (!v) {
        fprintf(stderr, "FAIL: %s: expected true\n", name);
        exit(1);
    }
}

static void expect_u16(const char *name, uint16_t want, uint16_t got)
{
    if (want != got) {
        fprintf(stderr, "FAIL: %s: want %04x got %04x\n", name, want, got);
        exit(1);
    }
}

static void expect_u8(const char *name, uint8_t want, uint8_t got)
{
    if (want != got) {
        fprintf(stderr, "FAIL: %s: want %02x got %02x\n", name, want, got);
        exit(1);
    }
}

static void expect_u64(const char *name, uint64_t want, uint64_t got)
{
    if (want != got) {
        fprintf(stderr, "FAIL: %s: want %llu got %llu\n",
                name,
                (unsigned long long)want,
                (unsigned long long)got);
        exit(1);
    }
}

static int poll_event(
    runtime_client *client,
    runtime_event *event,
    runtime_event_type type,
    double timeout_s)
{
    clock_t start = clock();
    while ((double)(clock() - start) / (double)CLOCKS_PER_SEC < timeout_s) {
        while (runtime_client_poll_event(client, event)) {
            if (event->type == RUNTIME_EVENT_ERROR) {
                fprintf(stderr, "runtime error: %s\n", event->data.error.message);
                exit(1);
            }
            if (event->type == type) {
                return 1;
            }
        }
    }
    return 0;
}

static void drain_events(runtime_client *client, double timeout_s)
{
    runtime_event event;
    clock_t start = clock();
    while ((double)(clock() - start) / (double)CLOCKS_PER_SEC < timeout_s) {
        if (!runtime_client_poll_event(client, &event)) {
            break;
        }
        if (event.type == RUNTIME_EVENT_ERROR) {
            fprintf(stderr, "runtime error: %s\n", event.data.error.message);
            exit(1);
        }
    }
}

static void wait_paused(runtime_client *client)
{
    runtime_event event;
    expect_true("PAUSED", poll_event(client, &event, RUNTIME_EVENT_PAUSED, 5.0));
}

static void fill_default_slots(runtime_config *config)
{
    int slot;
    for (slot = 0; slot < RUNTIME_APPLE_SLOT_COUNT; ++slot) {
        config->slot_cards[slot] = RUNTIME_SLOT_CARD_EMPTY;
        config->machine_config.slot_cards[slot] = RUNTIME_SLOT_CARD_EMPTY;
    }
    config->slot_cards[6] = RUNTIME_SLOT_CARD_DISKII;
    config->machine_config.slot_cards[6] = RUNTIME_SLOT_CARD_DISKII;
}

int main(void)
{
    runtime_config config;
    runtime *rt;
    runtime_client *client;
    runtime_event event;
    runtime_machine_config machine_config;
    const uint16_t code = 0x0600u;
    uint8_t initial_sp;

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fail("SDL_Init failed");
    }

    runtime_config_init(&config);
    fill_default_slots(&config);
    config.start_running = false;
    /* Opt-in; default is off so intentional BRKs keep running like hardware. */
    config.machine_config.pause_on_brk = true;

    rt = runtime_create(&config);
    expect_true("runtime_create", rt != NULL);
    expect_true("runtime_start", runtime_start(rt));
    client = runtime_get_client(rt);
    expect_true("client", client != NULL);

    expect_true("STARTED", poll_event(client, &event, RUNTIME_EVENT_STARTED, 2.0));
    wait_paused(client);
    expect_true("request cpu", runtime_client_request_cpu_state(client));
    expect_true(
        "CPU",
        poll_event(client, &event, RUNTIME_EVENT_CPU_STATE_RESPONSE, 2.0));
    initial_sp = event.data.cpu_state.sp;
    drain_events(client, 0.05);

    expect_true(
        "poke BRK",
        runtime_client_write_memory_byte(
            client, code, 0x00u, RUNTIME_MEMORY_MODE_MAIN));
    expect_true("set PC to BRK", runtime_client_set_pc(client, code));
    drain_events(client, 0.05);

    expect_true("run into BRK", runtime_client_run(client));
    expect_true("RUNNING", poll_event(client, &event, RUNTIME_EVENT_RUNNING, 2.0));
    /* a2m publishes MACHINE_STATE before PAUSED; capture either order. */
    {
        clock_t start = clock();
        int got_paused = 0;
        int got_machine = 0;
        runtime_event machine_event;

        memset(&machine_event, 0, sizeof(machine_event));
        while ((double)(clock() - start) / (double)CLOCKS_PER_SEC < 5.0) {
            while (runtime_client_poll_event(client, &event)) {
                if (event.type == RUNTIME_EVENT_ERROR) {
                    fprintf(stderr, "runtime error: %s\n", event.data.error.message);
                    exit(1);
                }
                if (event.type == RUNTIME_EVENT_PAUSED) {
                    got_paused = 1;
                }
                if (event.type == RUNTIME_EVENT_MACHINE_STATE_RESPONSE) {
                    machine_event = event;
                    got_machine = 1;
                }
            }
            if (got_paused && got_machine) {
                break;
            }
        }
        expect_true("PAUSED for BRK", got_paused);
        expect_true("machine snapshot", got_machine);
        expect_u64("paused", 0u, machine_event.data.machine_state.running);
        expect_u16("PC unchanged", code, machine_event.data.machine_state.pc);
        expect_u64(
            "stop reason BRK",
            (uint64_t)RUNTIME_STOP_REASON_BRK,
            (uint64_t)machine_event.data.machine_state.stop_reason);
        expect_u8("SP untouched", initial_sp, machine_event.data.machine_state.sp);
    }

    /* Live disable: free-run must execute BRK like hardware when the option is off. */
    memset(&machine_config, 0, sizeof(machine_config));
    machine_config.pause_on_brk = false;
    machine_config.apple_model = 0u;
    machine_config.slot_cards[6] = RUNTIME_SLOT_CARD_DISKII;
    expect_true(
        "disable pause_on_brk",
        runtime_client_apply_machine_config(
            client, &machine_config, NULL, NULL, NULL, false, false, false));
    drain_events(client, 0.1);

    expect_true("set PC again", runtime_client_set_pc(client, code));
    drain_events(client, 0.05);
    expect_true("run cycles through BRK", runtime_client_run_cycles(client, 16u));
    expect_true(
        "RUN_COMPLETE without BRK pause",
        poll_event(client, &event, RUNTIME_EVENT_RUN_COMPLETE, 5.0));
    expect_true("request machine after BRK", runtime_client_request_machine_state(client));
    expect_true(
        "machine after executed BRK",
        poll_event(client, &event, RUNTIME_EVENT_MACHINE_STATE_RESPONSE, 2.0));
    expect_u64(
        "stop reason not BRK",
        (uint64_t)RUNTIME_STOP_REASON_RUN_COMPLETE,
        (uint64_t)event.data.machine_state.stop_reason);
    /* BRK pushed PC+2 and P; SP must have decreased by 3. */
    expect_u8(
        "SP after executed BRK",
        (uint8_t)(initial_sp - 3u),
        event.data.machine_state.sp);

    runtime_destroy(rt);
    SDL_Quit();
    printf("ok\n");
    return 0;
}
