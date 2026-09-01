#include "c64_swiftlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, bool actual) {
    if (!actual) {
        fprintf(stderr, "FAIL: %s: expected true\n", name);
        exit(1);
    }
}

static void expect_false(const char *name, bool actual) {
    if (actual) {
        fprintf(stderr, "FAIL: %s: expected false\n", name);
        exit(1);
    }
}

static void expect_eq_u8(const char *name, uint8_t expected, uint8_t actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected 0x%02X, got 0x%02X\n", name, expected, actual);
        exit(1);
    }
}

static void expect_eq_u16(const char *name, uint16_t expected, uint16_t actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected %u, got %u\n", name, expected, actual);
        exit(1);
    }
}

static void expect_str(const char *name, const char *expected, const char *actual) {
    if (strcmp(expected, actual) != 0) {
        fprintf(stderr, "FAIL: %s: expected '%s', got '%s'\n", name, expected, actual);
        exit(1);
    }
}

static uint8_t status(c64_swiftlink *sl) {
    return c64_swiftlink_read(sl, (uint16_t)(sl->base + 1u));
}

static void service_pump(c64_swiftlink *sl) {
    int i;
    for (i = 0; i < 64; ++i) {
        c64_swiftlink_service(sl);
    }
}

static void write_bytes(c64_swiftlink *sl, const char *text) {
    size_t i;
    for (i = 0; text[i] != '\0'; ++i) {
        int spins = 0;
        while ((status(sl) & C64_SWIFTLINK_STATUS_TDRE) == 0) {
            c64_swiftlink_service(sl);
            if (++spins > 32) {
                fail("write_bytes: TDRE stuck");
            }
        }
        c64_swiftlink_write(sl, (uint16_t)(sl->base + 0u), (uint8_t)text[i]);
        c64_swiftlink_service(sl);
    }
}

static void drain_rx(c64_swiftlink *sl, char *out, size_t out_cap) {
    size_t n = 0;
    int idle = 0;

    if (out_cap == 0) {
        return;
    }
    out[0] = '\0';
    /* Pump until the RX ring+holding stay empty for a stretch of services. */
    while (idle < 64 && n + 1u < out_cap) {
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

static void expect_rx_contains(c64_swiftlink *sl, const char *needle) {
    char buf[256];
    drain_rx(sl, buf, sizeof(buf));
    if (strstr(buf, needle) == NULL) {
        fprintf(stderr,
                "FAIL: RX missing '%s' in '%s' (mode=%d rx_count=%u holding=%d)\n",
                needle,
                buf,
                (int)sl->mode,
                sl->rx_count,
                sl->rx_holding_full ? 1 : 0);
        exit(1);
    }
}

static void setup(c64_swiftlink *sl) {
    c64_swiftlink_init(sl);
    c64_swiftlink_set_enabled(sl, true);
}

static void test_at_ok(void) {
    c64_swiftlink sl;
    setup(&sl);
    write_bytes(&sl, "AT\r");
    expect_rx_contains(&sl, "OK\r");
}

static void test_ate_atv_syntax(void) {
    c64_swiftlink sl;
    char buf[64];

    setup(&sl);
    write_bytes(&sl, "ATE0\r");
    expect_rx_contains(&sl, "OK\r");
    write_bytes(&sl, "ATE=1\r");
    expect_rx_contains(&sl, "OK\r");

    write_bytes(&sl, "ATV0\r");
    drain_rx(&sl, buf, sizeof(buf));
    expect_true("numeric ok", strstr(buf, "0\r") != NULL);

    write_bytes(&sl, "ATV=1\r");
    expect_rx_contains(&sl, "OK\r");
}

static void test_atdt_default_port_and_connect(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;

    setup(&sl);
    write_bytes(&sl, "atdtexample.com\n");
    expect_true("req", c64_swiftlink_take_host_request(&sl, &req));
    expect_eq_u8("kind", (uint8_t)C64_SWIFTLINK_HOST_REQ_CONNECT, (uint8_t)req.kind);
    expect_str("host", "example.com", req.host);
    expect_eq_u16("port", 23, req.port);

    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    expect_rx_contains(&sl, "CONNECT\r");
    expect_true("carrier", sl.carrier_present);
    expect_eq_u8("online", (uint8_t)C64_SWIFTLINK_MODE_ONLINE, (uint8_t)sl.mode);
}

static void test_atdt_host_port(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;

    setup(&sl);
    write_bytes(&sl, "ATDT127.0.0.1:5000\r");
    expect_true("req", c64_swiftlink_take_host_request(&sl, &req));
    expect_str("host", "127.0.0.1", req.host);
    expect_eq_u16("port", 5000, req.port);
}

static void test_ath_matrices(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;
    char sink[64];

    setup(&sl);
    write_bytes(&sl, "ATH\r");
    expect_rx_contains(&sl, "OK\r");

    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    expect_true("dial", c64_swiftlink_take_host_request(&sl, &req));
    write_bytes(&sl, "ATH0\r");
    expect_rx_contains(&sl, "NO CARRIER\r");
    expect_true("hangup while dialing", c64_swiftlink_take_host_request(&sl, &req));
    expect_eq_u8("hangup kind", (uint8_t)C64_SWIFTLINK_HOST_REQ_HANGUP, (uint8_t)req.kind);

    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    expect_true("dial2", c64_swiftlink_take_host_request(&sl, &req));
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    drain_rx(&sl, sink, sizeof(sink));
    /* Online data path is TCP/escape only; hang up with +++ (ATH would be
       forwarded to the peer as payload until classic +++ command-mode exists). */
    write_bytes(&sl, "+++");
    expect_rx_contains(&sl, "NO CARRIER\r");
    expect_false("carrier clear", sl.carrier_present);
}

static void test_atz_matrices(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;
    char sink[64];

    setup(&sl);
    write_bytes(&sl, "ATE0\r");
    drain_rx(&sl, sink, sizeof(sink));
    write_bytes(&sl, "ATZ\r");
    expect_rx_contains(&sl, "OK\r");
    expect_true("echo restored", sl.echo);

    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    expect_true("dial", c64_swiftlink_take_host_request(&sl, &req));
    write_bytes(&sl, "ATZ\r");
    expect_rx_contains(&sl, "NO CARRIER\r");

    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    expect_true("dial2", c64_swiftlink_take_host_request(&sl, &req));
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    drain_rx(&sl, sink, sizeof(sink));
    write_bytes(&sl, "+++");
    expect_rx_contains(&sl, "NO CARRIER\r");
    expect_eq_u8("command mode", (uint8_t)C64_SWIFTLINK_MODE_COMMAND, (uint8_t)sl.mode);
}

static void test_plus_plus_plus_escape(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;
    uint8_t tx[8];
    char sink[64];

    setup(&sl);
    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    expect_true("dial", c64_swiftlink_take_host_request(&sl, &req));
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    drain_rx(&sl, sink, sizeof(sink));

    write_bytes(&sl, "+++");
    expect_rx_contains(&sl, "NO CARRIER\r");
    expect_eq_u8("no tx leak", 0, (uint8_t)c64_swiftlink_pull_tx(&sl, tx, sizeof(tx)));
    expect_true("hangup req", c64_swiftlink_take_host_request(&sl, &req));
    expect_eq_u8("hangup", (uint8_t)C64_SWIFTLINK_HOST_REQ_HANGUP, (uint8_t)req.kind);
}

static void test_plus_abort_flush(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;
    uint8_t tx[8];
    size_t n;
    char sink[64];

    setup(&sl);
    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    c64_swiftlink_take_host_request(&sl, &req);
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    drain_rx(&sl, sink, sizeof(sink));

    write_bytes(&sl, "++X");
    n = c64_swiftlink_pull_tx(&sl, tx, sizeof(tx));
    expect_eq_u8("flushed len", 3, (uint8_t)n);
    expect_eq_u8("f0", (uint8_t)'+', tx[0]);
    expect_eq_u8("f1", (uint8_t)'+', tx[1]);
    expect_eq_u8("f2", (uint8_t)'X', tx[2]);
    expect_eq_u8("still online", (uint8_t)C64_SWIFTLINK_MODE_ONLINE, (uint8_t)sl.mode);
}

static void test_connect_failures_and_peer_close(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;
    char sink[64];

    setup(&sl);
    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    c64_swiftlink_take_host_request(&sl, &req);
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_NO_DIALTONE);
    service_pump(&sl);
    expect_rx_contains(&sl, "NO DIALTONE\r");

    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    c64_swiftlink_take_host_request(&sl, &req);
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_NO_ANSWER);
    service_pump(&sl);
    expect_rx_contains(&sl, "NO ANSWER\r");

    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    c64_swiftlink_take_host_request(&sl, &req);
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    drain_rx(&sl, sink, sizeof(sink));
    c64_swiftlink_host_peer_closed(&sl);
    service_pump(&sl);
    expect_rx_contains(&sl, "NO CARRIER\r");
    expect_false("no hangup req on peer close", c64_swiftlink_take_host_request(&sl, &req));
}

static void test_status_write_silent(void) {
    c64_swiftlink sl;
    c64_swiftlink_host_req req;
    char buf[64];

    setup(&sl);
    write_bytes(&sl, "ATDT127.0.0.1:9\r");
    c64_swiftlink_take_host_request(&sl, &req);
    c64_swiftlink_host_connect_result(&sl, C64_SWIFTLINK_CONN_OK);
    service_pump(&sl);
    drain_rx(&sl, buf, sizeof(buf));

    c64_swiftlink_write(&sl, 0xDE01, 0xFF);
    service_pump(&sl);
    drain_rx(&sl, buf, sizeof(buf));
    if (strstr(buf, "NO CARRIER") != NULL || strstr(buf, "OK") != NULL) {
        fail("status write must be silent (no AT response)");
    }
    expect_true("hangup latched", c64_swiftlink_take_host_request(&sl, &req));
    expect_eq_u8("hangup", (uint8_t)C64_SWIFTLINK_HOST_REQ_HANGUP, (uint8_t)req.kind);
    expect_eq_u8("command", (uint8_t)C64_SWIFTLINK_MODE_COMMAND, (uint8_t)sl.mode);
}

static void test_overlong_line_error(void) {
    c64_swiftlink sl;
    char line[C64_SWIFTLINK_AT_LINE_MAX + 16];
    size_t i;

    setup(&sl);
    line[0] = 'A';
    line[1] = 'T';
    for (i = 2; i < C64_SWIFTLINK_AT_LINE_MAX + 4u; ++i) {
        line[i] = 'X';
    }
    line[i++] = '\r';
    line[i] = '\0';
    write_bytes(&sl, line);
    expect_rx_contains(&sl, "ERROR\r");
}

static void test_atv0_numeric_ath(void) {
    c64_swiftlink sl;
    char buf[64];

    setup(&sl);
    write_bytes(&sl, "ATV0\r");
    drain_rx(&sl, buf, sizeof(buf));
    write_bytes(&sl, "ATH\r");
    drain_rx(&sl, buf, sizeof(buf));
    expect_true("numeric 0", strstr(buf, "0\r") != NULL);
}

int main(void) {
    test_at_ok();
    test_ate_atv_syntax();
    test_atdt_default_port_and_connect();
    test_atdt_host_port();
    test_ath_matrices();
    test_atz_matrices();
    test_plus_plus_plus_escape();
    test_plus_abort_flush();
    test_connect_failures_and_peer_close();
    test_status_write_silent();
    test_overlong_line_error();
    test_atv0_numeric_ath();
    printf("OK\n");
    return 0;
}
