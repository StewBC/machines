#include "runtime_swiftlink.h"

#include "log.h"
#include "platform_socket.h"
#include "runtime_internal.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static bool ring_push(
    uint8_t *ring,
    size_t cap,
    size_t *head,
    size_t *count,
    uint8_t byte)
{
    if (*count >= cap) {
        return false;
    }
    ring[*head] = byte;
    *head = (*head + 1u) % cap;
    (*count)++;
    return true;
}

static bool ring_pop(
    uint8_t *ring,
    size_t cap,
    size_t *tail,
    size_t *count,
    uint8_t *out)
{
    if (*count == 0) {
        return false;
    }
    *out = ring[*tail];
    *tail = (*tail + 1u) % cap;
    (*count)--;
    return true;
}

static void clear_rings_locked(runtime_swiftlink_bridge *b)
{
    b->to_net_head = 0;
    b->to_net_tail = 0;
    b->to_net_count = 0;
    b->from_net_head = 0;
    b->from_net_tail = 0;
    b->from_net_count = 0;
}

static void clear_to_net_locked(runtime_swiftlink_bridge *b)
{
    b->to_net_head = 0;
    b->to_net_tail = 0;
    b->to_net_count = 0;
}

/* Peer closed: drop TX (socket is dead) but keep from_net so pending peer
   bytes can still reach the guest before NO CARRIER. */
static void note_peer_closed_locked(runtime_swiftlink_bridge *b)
{
    clear_to_net_locked(b);
    b->peer_eof = true;
}

static int bridge_thread_main(void *userdata)
{
    runtime_swiftlink_bridge *b = userdata;
    platform_socket_connection *conn = NULL;

    if (!platform_socket_startup()) {
        log_error("swiftlink bridge: platform_socket_startup failed");
    }

    for (;;) {
        runtime_swiftlink_cmd cmd = RUNTIME_SWIFTLINK_CMD_NONE;
        char host[C64_SWIFTLINK_HOST_MAX];
        uint16_t port = 0;
        bool stop = false;
        uint8_t tx_buf[512];
        size_t tx_n = 0;
        size_t i;

        mutex_lock(b->mu);
        stop = b->stop_requested;
        cmd = b->cmd;
        if (cmd == RUNTIME_SWIFTLINK_CMD_CONNECT) {
            memcpy(host, b->cmd_host, sizeof(host));
            port = b->cmd_port;
        }
        b->cmd = RUNTIME_SWIFTLINK_CMD_NONE;

        if (conn != NULL && b->to_net_count > 0) {
            while (tx_n < sizeof(tx_buf) &&
                   ring_pop(
                       b->to_net,
                       RUNTIME_SWIFTLINK_TO_NET_SIZE,
                       &b->to_net_tail,
                       &b->to_net_count,
                       &tx_buf[tx_n])) {
                tx_n++;
            }
        }
        mutex_unlock(b->mu);

        if (stop) {
            break;
        }

        if (cmd == RUNTIME_SWIFTLINK_CMD_HANGUP) {
            if (conn != NULL) {
                platform_socket_connection_destroy(conn);
                conn = NULL;
            }
            mutex_lock(b->mu);
            clear_rings_locked(b);
            b->peer_eof = false;
            mutex_unlock(b->mu);
        } else if (cmd == RUNTIME_SWIFTLINK_CMD_CONNECT) {
            runtime_swiftlink_result res;

            if (conn != NULL) {
                platform_socket_connection_destroy(conn);
                conn = NULL;
            }
            mutex_lock(b->mu);
            clear_rings_locked(b);
            b->peer_eof = false;
            mutex_unlock(b->mu);

            conn = platform_socket_connect(
                host, port, RUNTIME_SWIFTLINK_CONNECT_TIMEOUT_MS);
            if (conn == NULL) {
                /* DNS/socket failure vs remote refuse are not distinguished
                   cheaply after getaddrinfo+connect; treat as no answer when
                   host looked resolvable-ish, dialtone for empty/fail fast.
                   v1: map all connect failures to NO_ANSWER except null host. */
                res = (host[0] == '\0')
                    ? RUNTIME_SWIFTLINK_RES_NO_DIALTONE
                    : RUNTIME_SWIFTLINK_RES_NO_ANSWER;
            } else {
                res = RUNTIME_SWIFTLINK_RES_CONNECTED;
            }
            mutex_lock(b->mu);
            b->result = res;
            mutex_unlock(b->mu);
            if (res != RUNTIME_SWIFTLINK_RES_CONNECTED) {
                log_info(
                    "swiftlink bridge: connect %s:%u failed", host, (unsigned)port);
            } else {
                log_info(
                    "swiftlink bridge: connected %s:%u", host, (unsigned)port);
            }
        }

        if (conn != NULL && tx_n > 0) {
            size_t off = 0;
            while (off < tx_n) {
                int wrote = platform_socket_write(conn, tx_buf + off, tx_n - off);
                if (wrote > 0) {
                    off += (size_t)wrote;
                    continue;
                }
                if (wrote == 0) {
                    if (platform_socket_wait_writable(
                            conn, RUNTIME_SWIFTLINK_POLL_MS) < 0) {
                        wrote = -1;
                    } else {
                        continue;
                    }
                }
                if (wrote < 0) {
                    platform_socket_connection_destroy(conn);
                    conn = NULL;
                    mutex_lock(b->mu);
                    note_peer_closed_locked(b);
                    mutex_unlock(b->mu);
                    break;
                }
            }
        }

        if (conn != NULL) {
            size_t from_space;

            mutex_lock(b->mu);
            from_space = RUNTIME_SWIFTLINK_FROM_NET_SIZE - b->from_net_count;
            mutex_unlock(b->mu);

            if (from_space == 0) {
                /* Back-pressure: do not recv (and drop) while the host RX ring
                   is full; wait briefly so hangup/disable stay responsive.
                   Still observe peer close/error while stalled. */
                int ready =
                    platform_socket_wait_readable(conn, RUNTIME_SWIFTLINK_POLL_MS);
                if (ready < 0) {
                    platform_socket_connection_destroy(conn);
                    conn = NULL;
                    mutex_lock(b->mu);
                    note_peer_closed_locked(b);
                    mutex_unlock(b->mu);
                }
            } else {
                int ready =
                    platform_socket_wait_readable(conn, RUNTIME_SWIFTLINK_POLL_MS);
                if (ready < 0) {
                    platform_socket_connection_destroy(conn);
                    conn = NULL;
                    mutex_lock(b->mu);
                    note_peer_closed_locked(b);
                    mutex_unlock(b->mu);
                } else if (ready > 0) {
                    uint8_t rx_buf[512];
                    size_t want = sizeof(rx_buf);
                    int got;

                    if (want > from_space) {
                        want = from_space;
                    }
                    got = platform_socket_read(conn, rx_buf, want);
                    if (got == 0 || got == -1) {
                        platform_socket_connection_destroy(conn);
                        conn = NULL;
                        mutex_lock(b->mu);
                        note_peer_closed_locked(b);
                        mutex_unlock(b->mu);
                    } else if (got > 0) {
                        mutex_lock(b->mu);
                        for (i = 0; i < (size_t)got; ++i) {
                            if (!ring_push(
                                    b->from_net,
                                    RUNTIME_SWIFTLINK_FROM_NET_SIZE,
                                    &b->from_net_head,
                                    &b->from_net_count,
                                    rx_buf[i])) {
                                /* Should not happen: we capped want to space. */
                                break;
                            }
                        }
                        mutex_unlock(b->mu);
                    }
                    /* got == -2 would-block: ignore */
                }
            }
        } else {
            /* Idle: short sleep via readable wait on nothing — use poll on
               stop by sleeping through a timed mutex-less pause. */
#if defined(_WIN32)
            Sleep(RUNTIME_SWIFTLINK_POLL_MS);
#else
            {
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = (long)RUNTIME_SWIFTLINK_POLL_MS * 1000000L;
                nanosleep(&ts, NULL);
            }
#endif
        }
    }

    if (conn != NULL) {
        platform_socket_connection_destroy(conn);
    }
    return 0;
}

void runtime_swiftlink_bridge_init(runtime_swiftlink_bridge *b)
{
    if (b == NULL) {
        return;
    }
    memset(b, 0, sizeof(*b));
    b->mu = mutex_create();
}

void runtime_swiftlink_bridge_destroy(runtime_swiftlink_bridge *b)
{
    if (b == NULL) {
        return;
    }
    runtime_swiftlink_bridge_stop(b);
    mutex_destroy(b->mu);
    b->mu = NULL;
}

bool runtime_swiftlink_bridge_start(runtime_swiftlink_bridge *b)
{
    if (b == NULL || b->mu == NULL) {
        return false;
    }
    mutex_lock(b->mu);
    if (b->thread_running) {
        mutex_unlock(b->mu);
        return true;
    }
    b->stop_requested = false;
    b->cmd = RUNTIME_SWIFTLINK_CMD_NONE;
    b->result = RUNTIME_SWIFTLINK_RES_NONE;
    b->peer_eof = false;
    clear_rings_locked(b);
    mutex_unlock(b->mu);

    b->thread = thread_create("c64m-swiftlink", bridge_thread_main, b);
    if (b->thread == NULL) {
        log_error("swiftlink bridge: thread_create failed");
        return false;
    }
    mutex_lock(b->mu);
    b->thread_running = true;
    mutex_unlock(b->mu);
    return true;
}

void runtime_swiftlink_bridge_stop(runtime_swiftlink_bridge *b)
{
    thread *t;

    if (b == NULL || b->mu == NULL) {
        return;
    }

    mutex_lock(b->mu);
    if (!b->thread_running) {
        mutex_unlock(b->mu);
        return;
    }
    b->stop_requested = true;
    b->cmd = RUNTIME_SWIFTLINK_CMD_HANGUP;
    t = b->thread;
    mutex_unlock(b->mu);

    if (t != NULL) {
        thread_join(t);
        thread_destroy(t);
    }

    mutex_lock(b->mu);
    b->thread = NULL;
    b->thread_running = false;
    b->stop_requested = false;
    b->cmd = RUNTIME_SWIFTLINK_CMD_NONE;
    b->result = RUNTIME_SWIFTLINK_RES_NONE;
    b->peer_eof = false;
    clear_rings_locked(b);
    mutex_unlock(b->mu);
}

void runtime_swiftlink_bridge_pump(runtime_swiftlink_bridge *b, c64_swiftlink *sl)
{
    c64_swiftlink_host_req req;
    runtime_swiftlink_result res;
    uint8_t chunk[256];
    size_t n;
    size_t i;

    if (b == NULL || sl == NULL || !sl->enabled) {
        return;
    }

    c64_swiftlink_service(sl);

    if (c64_swiftlink_take_host_request(sl, &req)) {
        mutex_lock(b->mu);
        if (req.kind == C64_SWIFTLINK_HOST_REQ_CONNECT) {
            b->cmd = RUNTIME_SWIFTLINK_CMD_CONNECT;
            memcpy(b->cmd_host, req.host, sizeof(b->cmd_host));
            b->cmd_port = req.port;
        } else if (req.kind == C64_SWIFTLINK_HOST_REQ_HANGUP) {
            b->cmd = RUNTIME_SWIFTLINK_CMD_HANGUP;
        }
        mutex_unlock(b->mu);
    }

    mutex_lock(b->mu);
    res = b->result;
    b->result = RUNTIME_SWIFTLINK_RES_NONE;
    mutex_unlock(b->mu);

    if (res == RUNTIME_SWIFTLINK_RES_CONNECTED) {
        c64_swiftlink_host_connect_result(sl, C64_SWIFTLINK_CONN_OK);
    } else if (res == RUNTIME_SWIFTLINK_RES_NO_DIALTONE) {
        c64_swiftlink_host_connect_result(sl, C64_SWIFTLINK_CONN_NO_DIALTONE);
    } else if (res == RUNTIME_SWIFTLINK_RES_NO_ANSWER) {
        c64_swiftlink_host_connect_result(sl, C64_SWIFTLINK_CONN_NO_ANSWER);
    }

    {
        size_t to_space;

        mutex_lock(b->mu);
        to_space = RUNTIME_SWIFTLINK_TO_NET_SIZE - b->to_net_count;
        mutex_unlock(b->mu);
        if (to_space > sizeof(chunk)) {
            to_space = sizeof(chunk);
        }
        n = (to_space > 0) ? c64_swiftlink_pull_tx(sl, chunk, to_space) : 0;
        if (n > 0) {
            mutex_lock(b->mu);
            for (i = 0; i < n; ++i) {
                if (!ring_push(
                        b->to_net,
                        RUNTIME_SWIFTLINK_TO_NET_SIZE,
                        &b->to_net_head,
                        &b->to_net_count,
                        chunk[i])) {
                    /* Should not happen: pull was capped to free space. */
                    break;
                }
            }
            mutex_unlock(b->mu);
        }
    }

    {
        /* Move host→guest only as far as the machine RX ring can accept.
           Never pop-then-drop: a full machine FIFO is back-pressure, not overrun. */
        size_t space = c64_swiftlink_rx_space(sl);
        size_t moved = 0;

        while (moved < space && moved < sizeof(chunk)) {
            uint8_t byte;
            bool got;

            mutex_lock(b->mu);
            got = ring_pop(
                b->from_net,
                RUNTIME_SWIFTLINK_FROM_NET_SIZE,
                &b->from_net_tail,
                &b->from_net_count,
                &byte);
            mutex_unlock(b->mu);
            if (!got) {
                break;
            }
            chunk[moved++] = byte;
        }
        if (moved > 0) {
            size_t accepted = c64_swiftlink_push_rx(sl, chunk, moved);
            /* accepted must equal moved when space was checked first. */
            (void)accepted;
        }
    }

    /* Apply peer EOF only after from_net is empty so pending peer bytes
       (FICS goodbye, etc.) reach the guest before NO CARRIER. */
    {
        bool apply_eof = false;

        mutex_lock(b->mu);
        if (b->peer_eof && b->from_net_count == 0) {
            b->peer_eof = false;
            apply_eof = true;
        }
        mutex_unlock(b->mu);
        if (apply_eof) {
            c64_swiftlink_host_peer_closed(sl);
        }
    }

    c64_swiftlink_service(sl);
}

bool runtime_swiftlink_set_enabled(runtime *rt, bool enabled, uint16_t base)
{
    if (rt == NULL) {
        return false;
    }

    if (enabled) {
        uint16_t prev_base = rt->machine.swiftlink.base;
        bool prev_on = rt->machine.swiftlink.enabled;

        c64_swiftlink_set_base(&rt->machine.swiftlink, base);
        c64_swiftlink_set_enabled(&rt->machine.swiftlink, true);
        if (c64_swiftlink_conflicts(&rt->machine)) {
            c64_swiftlink_set_enabled(&rt->machine.swiftlink, prev_on);
            c64_swiftlink_set_base(&rt->machine.swiftlink, prev_base);
            runtime_publish_error(
                rt,
                "SwiftLink conflicts with mounted cartridge I/O page");
            return false;
        }
        if (!runtime_swiftlink_bridge_start(&rt->swiftlink)) {
            c64_swiftlink_set_enabled(&rt->machine.swiftlink, prev_on);
            c64_swiftlink_set_base(&rt->machine.swiftlink, prev_base);
            return false;
        }
        return true;
    }

    runtime_swiftlink_bridge_stop(&rt->swiftlink);
    c64_swiftlink_reset(&rt->machine.swiftlink);
    c64_swiftlink_set_enabled(&rt->machine.swiftlink, false);
    return true;
}

void runtime_swiftlink_pump(runtime *rt)
{
    if (rt == NULL) {
        return;
    }
    runtime_swiftlink_bridge_pump(&rt->swiftlink, &rt->machine.swiftlink);
}

void runtime_swiftlink_shutdown(runtime *rt)
{
    if (rt == NULL) {
        return;
    }
    runtime_swiftlink_set_enabled(rt, false, C64_SWIFTLINK_BASE_DE00);
}

void runtime_swiftlink_hangup(runtime *rt)
{
    runtime_swiftlink_bridge *b;

    if (rt == NULL) {
        return;
    }

    b = &rt->swiftlink;
    if (b->mu != NULL) {
        mutex_lock(b->mu);
        if (b->thread_running) {
            b->cmd = RUNTIME_SWIFTLINK_CMD_HANGUP;
            b->result = RUNTIME_SWIFTLINK_RES_NONE;
            b->peer_eof = false;
            clear_rings_locked(b);
        }
        mutex_unlock(b->mu);
    }

    c64_swiftlink_drop_host_session(&rt->machine.swiftlink);
}
