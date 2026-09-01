#include "c64_swiftlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_eq_u8(const char *name, uint8_t expected, uint8_t actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected 0x%02X, got 0x%02X\n", name, expected, actual);
        exit(1);
    }
}

static void expect_eq_u16(const char *name, uint16_t expected, uint16_t actual) {
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected 0x%04X, got 0x%04X\n", name, expected, actual);
        exit(1);
    }
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

static uint8_t status(c64_swiftlink *sl) {
    return c64_swiftlink_read(sl, (uint16_t)(sl->base + 1u));
}

static void test_init_defaults(void) {
    c64_swiftlink sl;

    c64_swiftlink_init(&sl);
    expect_false("init: enabled", sl.enabled);
    expect_eq_u16("init: base", C64_SWIFTLINK_BASE_DE00, sl.base);
    expect_eq_u8("init: TDRE set", C64_SWIFTLINK_STATUS_TDRE | C64_SWIFTLINK_STATUS_CD | C64_SWIFTLINK_STATUS_DSR,
                 status(&sl));
    expect_false("init: RDRF clear", (status(&sl) & C64_SWIFTLINK_STATUS_RDRF) != 0);
}

static void test_owns_and_base(void) {
    c64_swiftlink sl;

    c64_swiftlink_init(&sl);
    expect_false("owns disabled DE00", c64_swiftlink_owns(&sl, 0xDE00));
    c64_swiftlink_set_enabled(&sl, true);
    expect_true("owns DE00", c64_swiftlink_owns(&sl, 0xDE00));
    expect_true("owns DEFF", c64_swiftlink_owns(&sl, 0xDEFF));
    expect_false("owns DF00", c64_swiftlink_owns(&sl, 0xDF00));

    c64_swiftlink_set_base(&sl, 0xDF00);
    expect_eq_u16("base DF00", C64_SWIFTLINK_BASE_DF00, sl.base);
    expect_true("owns DF10", c64_swiftlink_owns(&sl, 0xDF10));
    expect_false("owns DE00 after DF", c64_swiftlink_owns(&sl, 0xDE00));

    c64_swiftlink_set_base(&sl, 0x1234); /* normalize to DE00 */
    expect_eq_u16("base invalid→DE00", C64_SWIFTLINK_BASE_DE00, sl.base);
}

static void test_register_readback_and_unmapped(void) {
    c64_swiftlink sl;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);

    c64_swiftlink_write(&sl, 0xDE02, 0x0B);
    /* $1C = internal generator + baud nibble nonzero (9600) → mode bit clear. */
    c64_swiftlink_write(&sl, 0xDE03, 0x1C);
    c64_swiftlink_write(&sl, 0xDE07, 0x02);
    expect_eq_u8("command", 0x0B, c64_swiftlink_read(&sl, 0xDE02));
    expect_eq_u8("control", 0x1C, c64_swiftlink_read(&sl, 0xDE03));
    expect_eq_u8("turbo232", 0x02, c64_swiftlink_read(&sl, 0xDE07));

    /* $10 = internal + baud nibble 0000 → Turbo232 enhanced; mode bit set. */
    c64_swiftlink_write(&sl, 0xDE03, 0x10);
    expect_eq_u8("turbo232 mode", 0x06, c64_swiftlink_read(&sl, 0xDE07));

    expect_eq_u8("unmapped read", 0xFF, c64_swiftlink_read(&sl, 0xDE04));
    c64_swiftlink_write(&sl, 0xDE04, 0x55); /* ignore */
    expect_eq_u8("unmapped still FF", 0xFF, c64_swiftlink_read(&sl, 0xDE04));
}

static void test_tdre_holding_and_ignore(void) {
    c64_swiftlink sl;
    uint8_t out[4];
    size_t n;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);

    expect_true("TDRE ready", (status(&sl) & C64_SWIFTLINK_STATUS_TDRE) != 0);
    c64_swiftlink_write(&sl, 0xDE00, 0x41);
    expect_false("TDRE clear after write", (status(&sl) & C64_SWIFTLINK_STATUS_TDRE) != 0);

    /* Second write while TDRE clear is ignored. */
    c64_swiftlink_write(&sl, 0xDE00, 0x42);
    c64_swiftlink_service(&sl);
    n = c64_swiftlink_pull_tx(&sl, out, sizeof(out));
    expect_eq_u8("pulled count", 1, (uint8_t)n);
    expect_eq_u8("pulled byte", 0x41, out[0]);
    expect_true("TDRE after service", (status(&sl) & C64_SWIFTLINK_STATUS_TDRE) != 0);
}

static void test_retromate_tdre_burst(void) {
    /* RetroMate fills a send buffer then polls TDRE before each sta data. */
    static const char line[] = "atdt127.0.0.1:1234\n";
    c64_swiftlink sl;
    uint8_t all[64];
    size_t i;
    size_t total;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);

    /* Guest-style init: control $10, turbo $00, command $0B */
    c64_swiftlink_write(&sl, 0xDE03, 0x10);
    c64_swiftlink_write(&sl, 0xDE07, 0x00);
    c64_swiftlink_write(&sl, 0xDE02, 0x0B);

    for (i = 0; i < sizeof(line) - 1u; ++i) {
        int spins = 0;
        while ((status(&sl) & C64_SWIFTLINK_STATUS_TDRE) == 0) {
            c64_swiftlink_service(&sl);
            if (++spins > 8) {
                fail("retromate burst: TDRE never returned");
            }
        }
        c64_swiftlink_write(&sl, 0xDE00, (uint8_t)line[i]);
        c64_swiftlink_service(&sl);
    }

    total = c64_swiftlink_pull_tx(&sl, all, sizeof(all));
    if (total != sizeof(line) - 1u) {
        fprintf(stderr, "FAIL: retromate burst length: expected %zu got %zu\n",
                sizeof(line) - 1u, total);
        exit(1);
    }
    if (memcmp(all, line, total) != 0) {
        fail("retromate burst payload mismatch");
    }
}

static void test_rdrf_and_overrun(void) {
    c64_swiftlink sl;
    uint8_t payload[C64_SWIFTLINK_RX_RING_SIZE + 4];
    size_t accepted;
    size_t i;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);

    payload[0] = 0x7E;
    expect_eq_u8("push 1", 1, (uint8_t)c64_swiftlink_push_rx(&sl, payload, 1));
    c64_swiftlink_service(&sl);
    expect_true("RDRF set", (status(&sl) & C64_SWIFTLINK_STATUS_RDRF) != 0);
    expect_eq_u8("data read", 0x7E, c64_swiftlink_read(&sl, 0xDE00));
    expect_false("RDRF clear", (status(&sl) & C64_SWIFTLINK_STATUS_RDRF) != 0);

    for (i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)i;
    }
    accepted = c64_swiftlink_push_rx(&sl, payload, sizeof(payload));
    expect_eq_u16("ring capacity", C64_SWIFTLINK_RX_RING_SIZE, (uint16_t)accepted);
    expect_true("overrun latched", (status(&sl) & C64_SWIFTLINK_STATUS_OVERRUN) != 0);
    /* Status read clears overrun. */
    expect_false("overrun cleared", (status(&sl) & C64_SWIFTLINK_STATUS_OVERRUN) != 0);
}

static void test_carrier_polarity(void) {
    c64_swiftlink sl;
    uint8_t st;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);

    st = status(&sl);
    expect_true("CD inactive (1)", (st & C64_SWIFTLINK_STATUS_CD) != 0);
    expect_false("DSR asserted (0)", (st & C64_SWIFTLINK_STATUS_DSR) != 0);

    c64_swiftlink_set_carrier(&sl, true);
    st = status(&sl);
    expect_false("CD asserted (0)", (st & C64_SWIFTLINK_STATUS_CD) != 0);
}

static void test_status_write_reset(void) {
    c64_swiftlink sl;
    uint8_t tx;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);
    c64_swiftlink_set_base(&sl, 0xDF00);
    c64_swiftlink_write(&sl, 0xDF02, 0x0B);
    c64_swiftlink_write(&sl, 0xDF03, 0x10);
    c64_swiftlink_write(&sl, 0xDF00, 0x99);
    c64_swiftlink_set_carrier(&sl, true);

    c64_swiftlink_write(&sl, 0xDF01, 0x00); /* any status write = reset */

    expect_true("enabled kept", sl.enabled);
    expect_eq_u16("base kept", C64_SWIFTLINK_BASE_DF00, sl.base);
    expect_eq_u8("command cleared", 0x00, c64_swiftlink_read(&sl, 0xDF02));
    expect_eq_u8("control cleared", 0x00, c64_swiftlink_read(&sl, 0xDF03));
    expect_true("TDRE after reset", (status(&sl) & C64_SWIFTLINK_STATUS_TDRE) != 0);
    expect_false("carrier cleared", sl.carrier_present);
    expect_eq_u8("tx ring empty", 0, (uint8_t)c64_swiftlink_pull_tx(&sl, &tx, 1));
}

static void test_service_backpressure(void) {
    c64_swiftlink sl;
    uint8_t junk = 0xAA;
    size_t i;

    c64_swiftlink_init(&sl);
    c64_swiftlink_set_enabled(&sl, true);

    /* Fill TX ring completely via holding+service. */
    for (i = 0; i < C64_SWIFTLINK_TX_RING_SIZE; ++i) {
        while ((status(&sl) & C64_SWIFTLINK_STATUS_TDRE) == 0) {
            fail("unexpected TDRE clear while filling");
        }
        c64_swiftlink_write(&sl, 0xDE00, (uint8_t)i);
        c64_swiftlink_service(&sl);
    }

    /* One more byte sits in holding; service cannot push → TDRE stays clear. */
    c64_swiftlink_write(&sl, 0xDE00, junk);
    c64_swiftlink_service(&sl);
    expect_false("TDRE backpressure", (status(&sl) & C64_SWIFTLINK_STATUS_TDRE) != 0);

    /* Drain one from the ring; service should accept holding. */
    {
        uint8_t out;
        expect_eq_u8("drain one", 1, (uint8_t)c64_swiftlink_pull_tx(&sl, &out, 1));
    }
    c64_swiftlink_service(&sl);
    expect_true("TDRE after drain", (status(&sl) & C64_SWIFTLINK_STATUS_TDRE) != 0);
}

int main(void) {
    test_init_defaults();
    test_owns_and_base();
    test_register_readback_and_unmapped();
    test_tdre_holding_and_ignore();
    test_retromate_tdre_burst();
    test_rdrf_and_overrun();
    test_carrier_polarity();
    test_status_write_reset();
    test_service_backpressure();
    printf("OK\n");
    return 0;
}
