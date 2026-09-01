#include "c64.h"
#include "c64_snapshot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CIA_REG_PORT_A = 0x00,
    CIA_REG_DDRA = 0x02,
    TEST_RESET_VECTOR = 0xe000,
};

static void expect_true(const char *name, bool value) {
    if (!value) {
        fprintf(stderr, "%s: expected true\n", name);
        exit(1);
    }
}

static void expect_u8(const char *name, uint8_t expected, uint8_t actual) {
    if (expected != actual) {
        fprintf(stderr, "%s: expected %02x, got %02x\n", name, expected, actual);
        exit(1);
    }
}

static void expect_u32(const char *name, uint32_t expected, uint32_t actual) {
    if (expected != actual) {
        fprintf(stderr, "%s: expected %u, got %u\n", name, expected, actual);
        exit(1);
    }
}

static void build_roms(c64_rom_set *roms) {
    c64_rom_set_init(roms);
    roms->has_basic = true;
    roms->has_kernal = true;
    roms->has_character = true;
    roms->kernal[0x1ffc] = (uint8_t)(TEST_RESET_VECTOR & 0xff);
    roms->kernal[0x1ffd] = (uint8_t)(TEST_RESET_VECTOR >> 8);
}

static void boot_machine(c64_t *machine) {
    c64_rom_set roms;
    char error[128];

    c64_init(machine);
    build_roms(&roms);
    expect_true("install_roms", c64_install_roms(machine, &roms, error, sizeof(error)));
    expect_true("reset", c64_reset(machine, error, sizeof(error)));
}

static void set_cia1_driven(c64_t *m, uint8_t ddra, uint8_t pra) {
    m->cia1.registers[CIA_REG_DDRA] = ddra;
    m->cia1.registers[CIA_REG_PORT_A] = pra;
}

static void test_mux_table(void) {
    c64_t machine;
    struct {
        uint8_t ddra;
        uint8_t pra;
        uint8_t expect_x;
        const char *name;
    } cases[] = {
        {0x00, 0xC0, 0xFF, "reset_undirected"},
        {0xC0, 0x40, 0x2A, "port1_exclusive"},
        {0xC0, 0x80, 0x54, "port2_exclusive"},
        {0xC0, 0xC0, 0xFF, "both_driven"},
        {0x40, 0x40, 0x2A, "port1_ddra40"},
        {0x40, 0xC0, 0x2A, "port1_pra_c0_only_pa6_driven"},
        {0x80, 0x80, 0x54, "port2_ddra80"},
    };
    size_t i;

    boot_machine(&machine);
    c64_set_mouse(&machine, 1u, 0x2Au, 0x11u, C64_JOYSTICK_FIRE);
    c64_set_mouse(&machine, 2u, 0x54u, 0x22u, C64_JOYSTICK_UP);
    /* Last set_mouse wins mouse_port; re-activate port1 for the port1 cases. */
    c64_set_mouse(&machine, 1u, 0x2Au, 0x11u, C64_JOYSTICK_FIRE);

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint8_t expect = cases[i].expect_x;

        /* Port2 cases need mouse_port == 2. */
        if (expect == 0x54u) {
            c64_set_mouse(&machine, 2u, 0x54u, 0x22u, C64_JOYSTICK_UP);
        } else if (expect == 0x2Au) {
            c64_set_mouse(&machine, 1u, 0x2Au, 0x11u, C64_JOYSTICK_FIRE);
        }
        set_cia1_driven(&machine, cases[i].ddra, cases[i].pra);
        expect_u8(cases[i].name, expect, sid_read(&machine.sid, 0xD419));
    }

    /* Inactive ⇒ $FF even with exclusive mux. */
    c64_set_mouse(&machine, 1u, 0x2Au, 0x11u, 0);
    set_cia1_driven(&machine, 0xC0, 0x40);
    c64_clear_mouse(&machine);
    expect_u8("inactive_after_clear", 0xFF, sid_read(&machine.sid, 0xD419));
    expect_u8("inactive_poty", 0xFF, sid_read(&machine.sid, 0xD41A));
}

static void test_buttons_and_pots(void) {
    c64_t machine;

    boot_machine(&machine);
    c64_set_mouse(&machine, 1u, 0xAA, 0x55, (uint8_t)(C64_JOYSTICK_FIRE | C64_JOYSTICK_UP));
    expect_u8("joy1_fire_up", (uint8_t)(C64_JOYSTICK_FIRE | C64_JOYSTICK_UP),
              machine.joystick1);
    expect_u8("joy2_untouched", 0x00, machine.joystick2);
    set_cia1_driven(&machine, 0xC0, 0x40);
    expect_u8("potx_port1", 0xAA, sid_read(&machine.sid, 0xD419));
    expect_u8("poty_port1", 0x55, sid_read(&machine.sid, 0xD41A));

    c64_set_mouse(&machine, 2u, 0x12, 0x34, C64_JOYSTICK_FIRE);
    expect_u8("joy2_fire", C64_JOYSTICK_FIRE, machine.joystick2);
    set_cia1_driven(&machine, 0xC0, 0x80);
    expect_u8("potx_port2", 0x12, sid_read(&machine.sid, 0xD419));
    expect_u8("poty_port2", 0x34, sid_read(&machine.sid, 0xD41A));
    /* Wrong mux ⇒ $FF even while active on the other port. */
    set_cia1_driven(&machine, 0xC0, 0x40);
    expect_u8("wrong_port_mux", 0xFF, sid_read(&machine.sid, 0xD419));

    c64_clear_mouse(&machine);
    expect_u8("clear_joy2", 0x00, machine.joystick2);
    expect_true("clear_inactive", !machine.mouse_active);
}

static void test_snapshot_v15_forces_inactive(void) {
    c64_t machine;
    c64_t loaded;
    uint8_t *blob;
    size_t size;
    size_t written;
    char error[128];
    c64_rom_set roms;

    boot_machine(&machine);
    c64_set_mouse(&machine, 1u, 0x2A, 0x3C, C64_JOYSTICK_FIRE);
    set_cia1_driven(&machine, 0xC0, 0x40);
    expect_u8("pre_save_potx", 0x2A, sid_read(&machine.sid, 0xD419));
    expect_true("pre_save_active", machine.mouse_active);

    size = c64_snapshot_size(&machine);
    expect_true("snapshot_size", size > 0);
    blob = (uint8_t *)malloc(size);
    expect_true("blob_alloc", blob != NULL);
    written = c64_snapshot_save(&machine, blob, size);
    expect_true("snapshot_save", written == size);
    expect_u32("snapshot_version", C64_SNAPSHOT_VERSION,
               (uint32_t)blob[4] | ((uint32_t)blob[5] << 8) |
                   ((uint32_t)blob[6] << 16) | ((uint32_t)blob[7] << 24));

    c64_init(&loaded);
    build_roms(&roms);
    expect_true("loaded_roms", c64_install_roms(&loaded, &roms, error, sizeof(error)));
    expect_true("loaded_reset", c64_reset(&loaded, error, sizeof(error)));
    expect_true("snapshot_load", c64_snapshot_load(&loaded, blob, size));
    free(blob);

    expect_true("post_load_inactive", !loaded.mouse_active);
    expect_u8("post_load_mouse_port", 0, loaded.mouse_port);
    expect_u8("post_load_potx0", 0xFF, loaded.pot_x[0]);
    expect_u8("post_load_poty0", 0xFF, loaded.pot_y[0]);
    set_cia1_driven(&loaded, 0xC0, 0x40);
    expect_u8("post_load_sid_potx", 0xFF, sid_read(&loaded.sid, 0xD419));
    /* Pot reader still bound to loaded machine. */
    c64_set_mouse(&loaded, 1u, 0x77, 0x88, 0);
    expect_u8("rebind_works", 0x77, sid_read(&loaded.sid, 0xD419));
}

int main(void) {
    test_mux_table();
    test_buttons_and_pots();
    test_snapshot_v15_forces_inactive();
    printf("ok\n");
    return 0;
}
