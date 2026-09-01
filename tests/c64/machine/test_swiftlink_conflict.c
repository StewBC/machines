#include "c64.h"
#include "c64_bus.h"
#include "c64_swiftlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, bool v) {
    if (!v) {
        fprintf(stderr, "FAIL: %s expected true\n", name);
        exit(1);
    }
}

static void expect_false(const char *name, bool v) {
    if (v) {
        fprintf(stderr, "FAIL: %s expected false\n", name);
        exit(1);
    }
}

static void expect_u8(const char *name, uint8_t exp, uint8_t act) {
    if (exp != act) {
        fprintf(stderr, "FAIL: %s expected %02x got %02x\n", name, exp, act);
        exit(1);
    }
}

static void fill_bank(uint8_t *bank, size_t n, uint8_t tag) {
    size_t i;
    for (i = 0; i < n; ++i) {
        bank[i] = tag;
    }
}

static void test_hw_claim_helpers(void) {
    expect_true("ocean io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_OCEAN));
    expect_true("magic desk io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_MAGIC_DESK));
    expect_true("funplay io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_FUNPLAY));
    expect_true("c64gs io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_C64GS));
    expect_true("dinamic io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_DINAMIC));
    expect_false("normal io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_NORMAL));
    expect_false("super games io1", c64_cartridge_hw_claims_io1(C64_CARTRIDGE_HW_SUPER_GAMES));

    expect_true("super games io2", c64_cartridge_hw_claims_io2(C64_CARTRIDGE_HW_SUPER_GAMES));
    expect_false("ocean io2", c64_cartridge_hw_claims_io2(C64_CARTRIDGE_HW_OCEAN));
}

static void test_decode_before_cart_io1(void) {
    c64_t machine;
    uint8_t banks[C64_CARTRIDGE_ROM_BANK_SIZE * 2];
    char error[128];

    c64_init(&machine);
    fill_bank(banks, sizeof(banks), 0xA5);
    expect_true(
        "attach ocean",
        c64_attach_ocean_cartridge(
            &machine, banks, 2, 0, 1, error, sizeof(error)));

    c64_swiftlink_set_base(&machine.swiftlink, C64_SWIFTLINK_BASE_DE00);
    c64_swiftlink_set_enabled(&machine.swiftlink, true);
    expect_true("conflicts DE+ocean", c64_swiftlink_conflicts(&machine));

    /* With SwiftLink owning IO1, ACIA command register is visible; cart
       bank latches must not consume the write. */
    c64_bus_write(&machine.bus, 0xDE02, 0x0B);
    expect_u8("command readback", 0x0B, c64_bus_read(&machine.bus, 0xDE02));
    expect_u8(
        "status TDRE+CD",
        C64_SWIFTLINK_STATUS_TDRE | C64_SWIFTLINK_STATUS_CD,
        c64_bus_read(&machine.bus, 0xDE01) &
            (uint8_t)(C64_SWIFTLINK_STATUS_TDRE | C64_SWIFTLINK_STATUS_CD |
                      C64_SWIFTLINK_STATUS_DSR));

    c64_swiftlink_set_enabled(&machine.swiftlink, false);
    expect_false("no conflict when off", c64_swiftlink_conflicts(&machine));
}

static void test_df00_ok_with_ocean(void) {
    c64_t machine;
    uint8_t banks[C64_CARTRIDGE_ROM_BANK_SIZE * 2];
    char error[128];

    c64_init(&machine);
    fill_bank(banks, sizeof(banks), 0x11);
    expect_true(
        "attach ocean",
        c64_attach_ocean_cartridge(
            &machine, banks, 2, 0, 1, error, sizeof(error)));

    c64_swiftlink_set_base(&machine.swiftlink, C64_SWIFTLINK_BASE_DF00);
    c64_swiftlink_set_enabled(&machine.swiftlink, true);
    expect_false("DF ok with ocean", c64_swiftlink_conflicts(&machine));
    expect_true("owns DF10", c64_swiftlink_owns(&machine.swiftlink, 0xDF10));
    expect_false("not DE", c64_swiftlink_owns(&machine.swiftlink, 0xDE00));
}

static void test_super_games_conflicts_df(void) {
    c64_t machine;
    uint8_t banks[C64_CARTRIDGE_ROM_BANK_SIZE * 4];
    char error[128];

    c64_init(&machine);
    fill_bank(banks, sizeof(banks), 0x22);
    expect_true(
        "attach super games",
        c64_attach_super_games_cartridge(
            &machine, banks, 4, 0, 0, error, sizeof(error)));
    expect_true("claims io2", c64_cart_claims_io2(&machine.bus));

    c64_swiftlink_set_base(&machine.swiftlink, C64_SWIFTLINK_BASE_DF00);
    c64_swiftlink_set_enabled(&machine.swiftlink, true);
    expect_true("conflicts DF+SG", c64_swiftlink_conflicts(&machine));

    c64_swiftlink_set_base(&machine.swiftlink, C64_SWIFTLINK_BASE_DE00);
    expect_false("DE ok with SG", c64_swiftlink_conflicts(&machine));
}

static void test_normal_cart_coexists(void) {
    c64_t machine;
    uint8_t roml[C64_CARTRIDGE_ROM_BANK_SIZE];
    char error[128];

    c64_init(&machine);
    fill_bank(roml, sizeof(roml), 0x33);
    expect_true(
        "attach normal",
        c64_attach_generic_cartridge(
            &machine, roml, sizeof(roml), NULL, 0, 0, 1, error, sizeof(error)));
    expect_false("normal no io1", c64_cart_claims_io1(&machine.bus));

    c64_swiftlink_set_base(&machine.swiftlink, C64_SWIFTLINK_BASE_DE00);
    c64_swiftlink_set_enabled(&machine.swiftlink, true);
    expect_false("coexist", c64_swiftlink_conflicts(&machine));
}

static void test_hw_precheck(void) {
    c64_t machine;

    c64_init(&machine);
    c64_swiftlink_set_enabled(&machine.swiftlink, true);
    c64_swiftlink_set_base(&machine.swiftlink, C64_SWIFTLINK_BASE_DE00);
    expect_true(
        "would conflict ocean",
        c64_swiftlink_conflicts_with_hw(&machine, C64_CARTRIDGE_HW_OCEAN));
    expect_false(
        "would not conflict normal",
        c64_swiftlink_conflicts_with_hw(&machine, C64_CARTRIDGE_HW_NORMAL));
}

int main(void) {
    test_hw_claim_helpers();
    test_decode_before_cart_io1();
    test_df00_ok_with_ocean();
    test_super_games_conflicts_df();
    test_normal_cart_coexists();
    test_hw_precheck();
    printf("OK\n");
    return 0;
}
