#include "c64_swiftlink.h"
#include "platform_socket.h"
#include "runtime_swiftlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, int cond) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        exit(1);
    }
}

static void sleep_ms(unsigned ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static uint8_t status(c64_swiftlink *sl) {
    return c64_swiftlink_read(sl, (uint16_t)(sl->base + 1u));
}

static void write_bytes(c64_swiftlink *sl, const char *text) {
    size_t i;
    for (i = 0; text[i] != '\0'; ++i) {
        int spins = 0;
        while ((status(sl) & C64_SWIFTLINK_STATUS_TDRE) == 0) {
            c64_swiftlink_service(sl);
            if (++spins > 64) {
                fail("TDRE stuck");
            }
        }
        c64_swiftlink_write(sl, (uint16_t)(sl->base + 0u), (uint8_t)text[i]);
        c64_swiftlink_service(sl);
    }
}

static void drain_rx(c64_swiftlink *sl, char *out, size_t cap) {
    size_t n = 0;
    int idle = 0;
    out[0] = '\0';
    while (idle < 64 && n + 1u < cap) {
        c64_swiftlink_service(sl);
        if (status(sl) & C64_SWIFTLINK_STATUS_RDRF) {
            out[n++] = (char)c64_swiftlink_read(sl, (uint16_t)(sl->base + 0u));
            out[n] = '\0';
            idle = 0;
        } else {
            idle++;
        }
    }
}

static void pump_until(
    runtime_swiftlink_bridge *b,
    c64_swiftlink *sl,
    int (*pred)(c64_swiftlink *),
    int max_ms)
{
    int waited = 0;
    while (waited < max_ms) {
        runtime_swiftlink_bridge_pump(b, sl);
        if (pred(sl)) {
            return;
        }
        sleep_ms(10);
        waited += 10;
    }
    fail("pump_until timeout");
}

static int mode_online(c64_swiftlink *sl) {
    return sl->mode == C64_SWIFTLINK_MODE_ONLINE;
}

static int mode_command(c64_swiftlink *sl) {
    return sl->mode == C64_SWIFTLINK_MODE_COMMAND && !sl->carrier_present;
}

int main(void) {
    platform_socket_listener *listener;
    platform_socket_connection *peer = NULL;
    runtime_swiftlink_bridge bridge;
    c64_swiftlink sl;
    uint16_t port;
    char dial[64];
    char rx[128];
    uint8_t tx[64];
    size_t n;
    int i;
    const char *payload = "hello-bridge";

    expect_true("startup", platform_socket_startup());
    listener = platform_socket_listen_localhost(0);
    expect_true("listen", listener != NULL);
    expect_true(
        "listen nonblock",
        platform_socket_listener_set_nonblocking(listener, true));
    port = platform_socket_listener_bound_port(listener);
    expect_true("port", port != 0);

    runtime_swiftlink_bridge_init(&bridge);
    expect_true("bridge mu", bridge.mu != NULL);
    expect_true("bridge start", runtime_swiftlink_bridge_start(&bridge));

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_base(&sl, C64_SWIFTLINK_BASE_DE00);
    c64_swiftlink_set_enabled(&sl, true);

    (void)snprintf(dial, sizeof(dial), "ATDT127.0.0.1:%u\r", (unsigned)port);
    write_bytes(&sl, dial);

    /* Accept while pumps advance CONNECT. */
    for (i = 0; i < 200 && peer == NULL; ++i) {
        runtime_swiftlink_bridge_pump(&bridge, &sl);
        peer = platform_socket_accept(listener);
        if (peer == NULL) {
            sleep_ms(10);
        }
    }
    expect_true("peer accept", peer != NULL);
    (void)platform_socket_set_nonblocking(peer, true);

    pump_until(&bridge, &sl, mode_online, 3000);
    drain_rx(&sl, rx, sizeof(rx));
    expect_true("CONNECT", strstr(rx, "CONNECT") != NULL);

    write_bytes(&sl, payload);
    for (i = 0; i < 100; ++i) {
        runtime_swiftlink_bridge_pump(&bridge, &sl);
        n = 0;
        {
            int got = platform_socket_read(peer, tx, sizeof(tx));
            if (got > 0) {
                n = (size_t)got;
                break;
            }
        }
        sleep_ms(10);
    }
    expect_true("peer got payload", n == strlen(payload));
    expect_true("payload match", memcmp(tx, payload, n) == 0);

    expect_true(
        "peer reply",
        platform_socket_write_all(peer, "WORLD", 5));
    for (i = 0; i < 100; ++i) {
        runtime_swiftlink_bridge_pump(&bridge, &sl);
        drain_rx(&sl, rx, sizeof(rx));
        if (strstr(rx, "WORLD") != NULL) {
            break;
        }
        sleep_ms(10);
    }
    expect_true("guest got WORLD", strstr(rx, "WORLD") != NULL);

    /* Peer close must keep unread from_net bytes, then NO CARRIER after drain.
       Do not pump the guest until after close so goodbye sits in from_net when
       EOF is noted (the race that used to wipe the FICS logout banner). */
    expect_true(
        "peer goodbye",
        platform_socket_write_all(
            peer, "(http://www.freechess.org).\n", 28));
    sleep_ms(150); /* bridge thread recv → from_net */
    platform_socket_connection_destroy(peer);
    peer = NULL;
    sleep_ms(100); /* bridge thread notes peer_eof, keeps from_net */
    {
        int saw_goodbye = 0;
        int saw_nocarrier = 0;
        int goodbye_before_nocarrier = 0;
        for (i = 0; i < 200; ++i) {
            runtime_swiftlink_bridge_pump(&bridge, &sl);
            drain_rx(&sl, rx, sizeof(rx));
            if (!saw_goodbye &&
                strstr(rx, "(http://www.freechess.org).") != NULL) {
                saw_goodbye = 1;
                if (!saw_nocarrier) {
                    goodbye_before_nocarrier = 1;
                }
            }
            if (strstr(rx, "NO CARRIER") != NULL) {
                saw_nocarrier = 1;
            }
            if (saw_goodbye && saw_nocarrier && mode_command(&sl)) {
                break;
            }
            sleep_ms(10);
        }
        expect_true("peer-close kept goodbye", saw_goodbye);
        expect_true("goodbye before NO CARRIER", goodbye_before_nocarrier);
        expect_true("peer-close NO CARRIER after", saw_nocarrier);
        expect_true("peer-close command mode", mode_command(&sl));
    }

    platform_socket_listener_destroy(listener);
    runtime_swiftlink_bridge_stop(&bridge);
    runtime_swiftlink_bridge_destroy(&bridge);
    platform_socket_shutdown();

    printf("OK\n");
    return 0;
}
