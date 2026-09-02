#include "c1541.h"
#include "c64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static int g_media_writes;

static void test_media_write_cb(void *user, uint64_t cycle, int device) {
    (void)user;
    (void)cycle;
    (void)device;
    g_media_writes++;
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

/* Fill ROM with NOPs (0xEA) and mark it loaded so advance_one_cycle proceeds. */
static void load_nop_rom(c1541 *drive) {
    memset(drive->rom, 0xEA, C1541_ROM_SIZE);
    drive->rom_loaded = 1;
}

/* ------------------------------------------------------------------ */
/* Phase 2: Bus map and lifecycle tests                                */
/* ------------------------------------------------------------------ */

static void test_ram_read_write(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);

    drive.ram[0x0000] = 0xAB;
    drive.ram[0x07FF] = 0xCD;

    expect_eq_u8("ram[0x0000]", 0xAB, drive.ram[0x0000]);
    expect_eq_u8("ram[0x07FF]", 0xCD, drive.ram[0x07FF]);

    printf("PASS: test_ram_read_write\n");
}

static void test_rom_not_loaded_nop(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    /* ROM not loaded — advance_one_cycle must be a no-op (no crash). */
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    printf("PASS: test_rom_not_loaded_nop\n");
}

static void test_rom_loaded_flag(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    if (drive.rom_loaded != 0) fail("rom_loaded should be 0 after init");
    load_nop_rom(&drive);
    if (drive.rom_loaded != 1) fail("rom_loaded should be 1 after load_nop_rom");
    printf("PASS: test_rom_loaded_flag\n");
}

static void test_device_number(void) {
    static c64_t c64;
    static c1541 d8, d9;
    c64_init(&c64);
    c1541_init(&d8, &c64, 8);
    c1541_init(&d9, &c64, 9);
    if (d8.device_number != 8) fail("d8 device_number != 8");
    if (d9.device_number != 9) fail("d9 device_number != 9");
    printf("PASS: test_device_number\n");
}

static void test_destroy_zeroes(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    drive.rom_loaded = 1;
    c1541_destroy(&drive);
    if (drive.rom_loaded != 0) fail("destroy should zero rom_loaded");
    if (drive.device_number != 0) fail("destroy should zero device_number");
    printf("PASS: test_destroy_zeroes\n");
}

static void test_debug_read_map(void) {
    static c64_t c64;
    static c1541 drive;
    uint8_t value;

    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);

    drive.ram[0x0010] = 0x12u;
    drive.ram[0x07FF] = 0x34u;
    drive.rom[0x0000] = 0x56u;
    drive.via1.ifr = 0x43u;
    drive.via1.ier = 0x40u;

    if (!c1541_debug_read_map(&drive, 0x0010u, &value))
        fail("debug map should expose drive RAM");
    expect_eq_u8("debug RAM byte", 0x12u, value);

    if (!c1541_debug_read_map(&drive, 0x0FFFu, &value))
        fail("debug map should expose RAM mirror");
    expect_eq_u8("debug RAM mirror", 0x34u, value);

    if (!c1541_debug_read_map(&drive, 0x180Du, &value))
        fail("debug map should expose VIA1");
    expect_eq_u8("debug VIA1 IFR", 0xC3u, value);
    expect_eq_u8("debug VIA1 read side-effect", 0x43u, drive.via1.ifr);

    if (!c1541_debug_read_map(&drive, 0xC000u, &value))
        fail("debug map should expose loaded ROM");
    expect_eq_u8("debug ROM byte", 0x56u, value);

    if (c1541_debug_read_map(&drive, 0x1000u, &value))
        fail("debug map should mark unmapped gap invalid");
    expect_eq_u8("debug unmapped value", 0x00u, value);

    if (!c64_debug_read_drive_map(&c64, 8, 0x0010u, &value))
        fail("c64 drive map wrapper should expose drive 8 RAM");

    printf("PASS: test_debug_read_map\n");
}

/* ------------------------------------------------------------------ */
/* Phase 3A: IEC bus wiring                                            */
/* ------------------------------------------------------------------ */

/* Soft power alone no longer places a unit on the IEC bus. These unit tests
   poke a standalone c1541 while writing through c64_set_iec_drive_pull; the
   slot must be IMAGE+mounted so c64_drive_bus_pull includes that pull. */
static void arm_slot_for_iec(c64_t *c64, uint8_t device) {
    int slot = (int)(device - C64_DRIVE_MIN_DEVICE);
    c64_drive_slot *s;
    if (slot < 0 || slot >= C64_DRIVE_SLOT_COUNT) {
        return;
    }
    s = &c64->drives[slot];
    s->powered = true;
    s->mounted = true;
    s->backend = C64_DRIVE_BACKEND_IMAGE;
    s->image_kind = C64_DRIVE_IMAGE_D64;
}

/* Advancing one cycle with ROM loaded synchronises serial VIA output → C64 pull.
   VIA1 PB1 = DATA out; DDRB=0x02 (output), ORB bit 1 high → 1541 pulls DATA.
   After advance, c64.iec_external_pull should have C64_IEC_DATA set. */
static void test_iec_drive_pulls_data(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    (void)c64_power_on_drive(&c64, 8);
    arm_slot_for_iec(&c64, 8);

    drive.via1.ddrb = 0x02u; /* bit 1 = DATA output */
    drive.via1.orb  = 0x02u; /* bit 1 = 1 → pulls DATA low */

    /* IEC outputs pass a 2-stage pipeline; direct VIA pokes need settle cycles. */
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);

    if (!(c64.iec_external_pull & C64_IEC_DATA))
        fail("iec_drive_pulls_data: DATA should be pulled");
    if (c64.iec_external_pull & C64_IEC_CLK)
        fail("iec_drive_pulls_data: CLK should not be pulled");
    if (c64.iec_external_pull & C64_IEC_ATN)
        fail("iec_drive_pulls_data: ATN should not be pulled");

    printf("PASS: test_iec_drive_pulls_data\n");
}

/* When 1541 releases DATA (PB1 output = 0), iec_external_pull should clear DATA. */
static void test_iec_drive_releases_data(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    (void)c64_power_on_drive(&c64, 8);
    arm_slot_for_iec(&c64, 8);

    drive.via1.ddrb = 0x02u; /* bit 1 = DATA output */
    drive.via1.orb  = 0x00u; /* bit 1 = 0 → not driving DATA low */

    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);

    if (c64.iec_external_pull & C64_IEC_DATA)
        fail("iec_drive_releases_data: DATA should not be pulled");

    printf("PASS: test_iec_drive_releases_data\n");
}

static void test_cpu_instruction_cycles_are_throttled(void) {
    static c64_t c64;
    static c1541 drive;
    uint16_t pc_after_first;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);

    drive.cpu.cpu.pc = 0xC000u;

    c1541_advance_one_cycle(&drive);
    pc_after_first = drive.cpu.cpu.pc;
    c1541_advance_one_cycle(&drive);

    expect_eq_u16("throttled NOP PC", pc_after_first, drive.cpu.cpu.pc);
    if (drive.cpu_cycles_remaining != 0)
        fail("throttled NOP should consume its second cycle after two advances");

    c1541_advance_one_cycle(&drive);
    expect_eq_u16("next NOP PC", (uint16_t)(pc_after_first + 1u), drive.cpu.cpu.pc);

    printf("PASS: test_cpu_instruction_cycles_are_throttled\n");
}

static void test_via1_ca1_routes_to_irq(void) {
    static c64_t c64;
    static c1541 drive;
    int i;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);

    drive.cpu.cpu.pc = 0xC000u;
    drive.cpu.cpu.flags &= (uint8_t)~0x04u; /* IRQs enabled */
    drive.via1.ifr |= 0x02u; /* serial VIA CA1 flag */
    drive.via1.ier |= 0x02u; /* serial VIA CA1 enabled */

    for (i = 0; i < 32; i++) {
        c1541_advance_one_cycle(&drive);
    }

    if (drive.cpu.cpu.irq_entries != 1u)
        fail("serial VIA CA1 should enter the CPU IRQ path");
    if (drive.cpu.cpu.nmi_entries != 0u)
        fail("serial VIA CA1 must not enter the CPU NMI path");

    printf("PASS: test_via1_ca1_routes_to_irq\n");
}

static void test_via2_timer_pb7_sets_cpu_overflow(void) {
    static c64_t c64;
    static c1541 drive;
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);

    drive.cpu.cpu.pc = 0xC000u;
    drive.cpu.cpu.V = 0;
    drive.via2.acr = 0x80u;       /* T1 PB7 output enable */
    drive.via2.t1_running = 1;
    drive.via2.t1_counter = 0;    /* underflows on next VIA step */
    drive.via2.t1_latch = 0x0010u;
    drive.via2_t1_pb7_last = drive.via2.t1_pb7_state;

    c1541_advance_one_cycle(&drive);

    if (!drive.cpu.cpu.V)
        fail("VIA2 T1 PB7 toggle should set CPU overflow flag");

    printf("PASS: test_via2_timer_pb7_sets_cpu_overflow\n");
}

/* Drive 8 and drive 9 are separate open-collector pullers.  Stepping an idle
   drive 9 must not clear a line that drive 8 is still pulling low. */
static void test_iec_two_drive_pull_aggregation(void) {
    static c64_t c64;
    static c1541 d8, d9;
    c64_init(&c64);
    c1541_init(&d8, &c64, 8);
    c1541_init(&d9, &c64, 9);
    load_nop_rom(&d8);
    load_nop_rom(&d9);
    c1541_reset(&d8);
    c1541_reset(&d9);
    (void)c64_power_on_drive(&c64, 8);
    (void)c64_power_on_drive(&c64, 9);
    arm_slot_for_iec(&c64, 8);
    arm_slot_for_iec(&c64, 9);

    d8.via1.ddrb = 0x02u; /* bit 1 = DATA output */
    d8.via1.orb  = 0x02u; /* drive 8 pulls DATA low */
    d9.via1.ddrb = 0x02u; /* bit 1 = DATA output */
    d9.via1.orb  = 0x00u; /* drive 9 releases DATA */

    /* Pipeline settle (3 cycles) on each drive before observing the bus. */
    c1541_advance_one_cycle(&d8);
    c1541_advance_one_cycle(&d8);
    c1541_advance_one_cycle(&d8);
    c1541_advance_one_cycle(&d9);
    c1541_advance_one_cycle(&d9);
    c1541_advance_one_cycle(&d9);

    if (!(c64.iec_external_pull & C64_IEC_DATA))
        fail("iec_two_drive_pull_aggregation: drive 9 cleared drive 8 DATA pull");

    d8.via1.orb = 0x00u; /* now both drives release DATA */
    c1541_advance_one_cycle(&d8);
    c1541_advance_one_cycle(&d8);
    c1541_advance_one_cycle(&d8);
    c1541_advance_one_cycle(&d9);
    c1541_advance_one_cycle(&d9);
    c1541_advance_one_cycle(&d9);

    if (c64.iec_external_pull & C64_IEC_DATA)
        fail("iec_two_drive_pull_aggregation: DATA should release after both drives release");

    printf("PASS: test_iec_two_drive_pull_aggregation\n");
}

/* c64_get_iec_c64_pull reflects CIA2 Port A state (CIA2 bit 5 → C64_IEC_DATA).
   CIA_REG_PORT_A = 0x00, CIA_REG_DDRA = 0x02. */
static void test_iec_c64_pull_data(void) {
    static c64_t c64;
    uint8_t pull;

    c64_init(&c64);

    /* CIA2 IEC outputs feed open-collector inverters:
       DDRA bit 5 = output (0x20), ORA bit 5 = 1 → pull DATA. */
    c64.cia2.registers[0x00] = 0x20u; /* ORA bit 5 = 1 */
    c64.cia2.registers[0x02] = 0x20u; /* DDRA bit 5 = output */

    pull = c64_get_iec_c64_pull(&c64);
    if (!(pull & C64_IEC_DATA))
        fail("iec_c64_pull_data: CIA2 should pull DATA low");

    printf("PASS: test_iec_c64_pull_data\n");
}

static void test_iec_atn_ack_pulls_data(void) {
    static c64_t c64;
    static c1541 drive;

    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    (void)c64_power_on_drive(&c64, 8);
    arm_slot_for_iec(&c64, 8);

    /* CIA2 PA3 output high asserts ATN; serial VIA PB4 low acknowledges ATN by
       pulling DATA even if PB1 is not yet set by the firmware IRQ handler. */
    c64.cia2.registers[0x00] = 0x08u;
    c64.cia2.registers[0x02] = 0x08u;
    drive.via1.ddrb = 0x10u;
    drive.via1.orb = 0x00u;

    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);

    if (!(c64.iec_external_pull & C64_IEC_DATA))
        fail("iec_atn_ack_pulls_data: ATN acknowledge should pull DATA");

    drive.via1.orb = 0x10u;
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);

    if (c64.iec_external_pull & C64_IEC_DATA)
        fail("iec_atn_ack_pulls_data: PB4 high should release ATN acknowledge DATA");

    printf("PASS: test_iec_atn_ack_pulls_data\n");
}

/* Returns a freshly allocated 174848-byte D64 image with track 1 sector 0
   (byte offset 0) filled with `pattern`. Caller owns the memory. */
static uint8_t *make_test_d64(uint8_t pattern) {
    uint8_t *img = (uint8_t *)calloc(1, C64_DRIVE_D64_STANDARD_SIZE);
    if (img == NULL)
        fail("make_test_d64: out of memory");
    memset(img, pattern, 256);
    return img;
}

/* ------------------------------------------------------------------ */
/* Phase 4: Job-level write intercept                                  */
/* ------------------------------------------------------------------ */

/* WRITE job on a writable image: buffer is persisted into image_bytes and the
   slot is marked dirty. */
static void test_queued_write_job_success(void) {
    static c64_t c64;
    static c1541 drive;
    uint8_t *img;
    const c64_drive_slot *slot;
    c64_drive_status_result result;
    int i;

    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    g_media_writes = 0;
    c64_set_media_event_callback(&c64, test_media_write_cb, NULL);

    img = make_test_d64(0x11);
    result = c64_mount_d64_ex(
        &c64, 8, img, C64_DRIVE_D64_STANDARD_SIZE,
        NULL, 0, "test", "TEST", "AA", "2A", 664, true /* writable */);
    free(img);
    if (result != C64_DRIVE_STATUS_OK)
        fail("test_queued_write_job_success: c64_mount_d64_ex failed");

    /* WRITE job in buffer 2, track 1 sector 0 (D64 offset 0). */
    for (i = 0; i < 256; i++) drive.ram[0x0500 + i] = 0x7E;
    drive.ram[0x02] = 0x90u; /* WRITE job */
    drive.ram[0x0A] = 1;     /* hdrs[2] track  */
    drive.ram[0x0B] = 0;     /* hdrs[2] sector */
    drive.cpu.cpu.pc = 0xF2BEu;

    c1541_advance_one_cycle(&drive);

    expect_eq_u8("queued write job result", 0x01u, drive.ram[0x02]);
    if (g_media_writes < 1)
        fail("test_queued_write_job_success: guest write did not notify");

    slot = c64_get_drive_slot(&c64, 8);
    if (!slot || !slot->image_bytes)
        fail("test_queued_write_job_success: no slot image");
    if (!slot->dirty)
        fail("test_queued_write_job_success: slot not marked dirty");
    for (i = 0; i < 256; i++) {
        if (slot->image_bytes[i] != 0x7E)
            fail("test_queued_write_job_success: image not updated");
    }

    printf("PASS: test_queued_write_job_success\n");
}

/* WRITE job on a read-only image: nothing is written, the slot stays clean,
   and the job reports the write-protect status (DOS error 26). */
static void test_queued_write_job_write_protect(void) {
    static c64_t c64;
    static c1541 drive;
    uint8_t *img;
    const c64_drive_slot *slot;
    c64_drive_status_result result;
    int i;

    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    g_media_writes = 0;
    c64_set_media_event_callback(&c64, test_media_write_cb, NULL);

    img = make_test_d64(0x22);
    result = c64_mount_d64(
        &c64, 8, img, C64_DRIVE_D64_STANDARD_SIZE,
        NULL, 0, "test", "TEST", "AA", "2A", 664); /* read-only */
    free(img);
    if (result != C64_DRIVE_STATUS_OK)
        fail("test_queued_write_job_write_protect: c64_mount_d64 failed");

    for (i = 0; i < 256; i++) drive.ram[0x0500 + i] = 0x7E;
    drive.ram[0x02] = 0x90u; /* WRITE job */
    drive.ram[0x0A] = 1;
    drive.ram[0x0B] = 0;
    drive.cpu.cpu.pc = 0xF2BEu;

    c1541_advance_one_cycle(&drive);

    expect_eq_u8("write-protect job result", 0x08u, drive.ram[0x02]);
    if (g_media_writes != 0)
        fail("test_queued_write_job_write_protect: refused write notified");

    slot = c64_get_drive_slot(&c64, 8);
    if (!slot || !slot->image_bytes)
        fail("test_queued_write_job_write_protect: no slot image");
    if (slot->dirty)
        fail("test_queued_write_job_write_protect: slot marked dirty on read-only");
    for (i = 0; i < 256; i++) {
        if (slot->image_bytes[i] != 0x22)
            fail("test_queued_write_job_write_protect: image mutated on read-only");
    }

    printf("PASS: test_queued_write_job_write_protect\n");
}

/* WRITE job to an out-of-range sector leaves the image untouched and errors. */
static void test_queued_write_job_out_of_range(void) {
    static c64_t c64;
    static c1541 drive;
    uint8_t *img;
    const c64_drive_slot *slot;
    c64_drive_status_result result;
    int i;

    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);

    img = make_test_d64(0x33);
    result = c64_mount_d64_ex(
        &c64, 8, img, C64_DRIVE_D64_STANDARD_SIZE,
        NULL, 0, "test", "TEST", "AA", "2A", 664, true);
    free(img);
    if (result != C64_DRIVE_STATUS_OK)
        fail("test_queued_write_job_out_of_range: c64_mount_d64_ex failed");

    for (i = 0; i < 256; i++) drive.ram[0x0500 + i] = 0x7E;
    drive.ram[0x02] = 0x90u; /* WRITE job */
    drive.ram[0x0A] = 40;    /* track 40 does not exist on a 35-track D64 */
    drive.ram[0x0B] = 0;
    drive.cpu.cpu.pc = 0xF2BEu;

    c1541_advance_one_cycle(&drive);

    expect_eq_u8("out-of-range write job result", 0x02u, drive.ram[0x02]);

    slot = c64_get_drive_slot(&c64, 8);
    if (!slot || !slot->image_bytes)
        fail("test_queued_write_job_out_of_range: no slot image");
    if (slot->dirty)
        fail("test_queued_write_job_out_of_range: slot marked dirty on failed write");

    printf("PASS: test_queued_write_job_out_of_range\n");
}

int main(void) {
    /* Phase 2 */
    test_ram_read_write();
    test_rom_not_loaded_nop();
    test_rom_loaded_flag();
    test_device_number();
    test_destroy_zeroes();
    test_debug_read_map();

    /* Phase 3 */
    test_iec_drive_pulls_data();
    test_iec_drive_releases_data();
    test_cpu_instruction_cycles_are_throttled();
    test_via1_ca1_routes_to_irq();
    test_via2_timer_pb7_sets_cpu_overflow();
    test_iec_two_drive_pull_aggregation();
    test_iec_c64_pull_data();
    test_iec_atn_ack_pulls_data();
    test_queued_write_job_success();
    test_queued_write_job_write_protect();
    test_queued_write_job_out_of_range();

    printf("All c1541 tests passed.\n");
    return 0;
}
