#pragma once

#include "c64_swiftlink.h"
#include "mutex.h"
#include "thread.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct runtime;

enum {
    RUNTIME_SWIFTLINK_CONNECT_TIMEOUT_MS = 10000u,
    RUNTIME_SWIFTLINK_POLL_MS = 50u,
    RUNTIME_SWIFTLINK_TO_NET_SIZE = 4096u,
    RUNTIME_SWIFTLINK_FROM_NET_SIZE = 8192u
};

typedef enum runtime_swiftlink_cmd {
    RUNTIME_SWIFTLINK_CMD_NONE = 0,
    RUNTIME_SWIFTLINK_CMD_CONNECT,
    RUNTIME_SWIFTLINK_CMD_HANGUP
} runtime_swiftlink_cmd;

typedef enum runtime_swiftlink_result {
    RUNTIME_SWIFTLINK_RES_NONE = 0,
    RUNTIME_SWIFTLINK_RES_CONNECTED,
    RUNTIME_SWIFTLINK_RES_NO_DIALTONE,
    RUNTIME_SWIFTLINK_RES_NO_ANSWER,
    RUNTIME_SWIFTLINK_RES_PEER_CLOSED
} runtime_swiftlink_result;

typedef struct runtime_swiftlink_bridge {
    thread *thread;
    mutex *mu;
    bool stop_requested;
    bool thread_running;

    runtime_swiftlink_cmd cmd;
    char cmd_host[C64_SWIFTLINK_HOST_MAX];
    uint16_t cmd_port;

    runtime_swiftlink_result result;

    /* Sticky peer EOF: set when the TCP peer closes. from_net is kept until the
       runtime pump drains it to the guest; only then is PEER_CLOSED applied so
       goodbye banners (e.g. FICS quit) are not wiped or buried under NO CARRIER. */
    bool peer_eof;

    uint8_t to_net[RUNTIME_SWIFTLINK_TO_NET_SIZE];
    size_t to_net_head;
    size_t to_net_tail;
    size_t to_net_count;

    uint8_t from_net[RUNTIME_SWIFTLINK_FROM_NET_SIZE];
    size_t from_net_head;
    size_t from_net_tail;
    size_t from_net_count;
} runtime_swiftlink_bridge;

void runtime_swiftlink_bridge_init(runtime_swiftlink_bridge *b);
void runtime_swiftlink_bridge_destroy(runtime_swiftlink_bridge *b);

bool runtime_swiftlink_bridge_start(runtime_swiftlink_bridge *b);
void runtime_swiftlink_bridge_stop(runtime_swiftlink_bridge *b);

/* Runtime-thread only: service ACIA/Hayes and exchange bytes/cmds with bridge. */
void runtime_swiftlink_bridge_pump(runtime_swiftlink_bridge *b, c64_swiftlink *sl);

bool runtime_swiftlink_set_enabled(
    struct runtime *rt,
    bool enabled,
    uint16_t base,
    c64_swiftlink_irq_mode irq_mode,
    bool pace_baud);
void runtime_swiftlink_pump(struct runtime *rt);
void runtime_swiftlink_shutdown(struct runtime *rt);

/* Runtime-thread only: hang up TCP (if bridge live), clear bridge+machine
   FIFOs/CD, force command mode. Does not change host enable/base. Used after
   load-state and Inspector land/re-execute snapshot restores. */
void runtime_swiftlink_hangup(struct runtime *rt);

#ifdef __cplusplus
}
#endif
