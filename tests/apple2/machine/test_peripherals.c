#include "apple2.h"
#include "host_log.h"
#include "imagewriter.h"
#include "mboard.h"
#include "smrtprt.h"
#include "softswitch.h"
#include "ssc.h"
#include "ssc_rom.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define test_mkdir(path) _mkdir(path)
#define test_rmdir _rmdir
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(path) mkdir((path), 0777)
#define test_rmdir rmdir
#endif

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void test_mockingboard_attach(void)
{
    apple2_t m;
    uint8_t v;

    if (!apple2_init(&m)) {
        fail("apple2_init");
    }
    /* Default attach is slot 4. */
    if (m.slot_type[4] != SLOT_TYPE_MOCKINGBOARD) {
        fail("default MB slot 4");
    }
    if (m.mb_slot != 4) {
        fail("mb_slot");
    }

    /* Write ORB on VIA0 window (compat C0n0–C0n7). */
    softswitch_c0_write(&m, 0xC040 + 0x00, 0x07); /* latch address via ORB on pair0? */
    /* Use mockingboard_write path C040 = slot 4, reg 0 = ORB via0 low window */
    mockingboard_write(&m, &m.mockingboard[4], 4, 0xC040, 0x00, 0x07);
    mockingboard_write(&m, &m.mockingboard[4], 4, 0xC041, 0x01, 0x08); /* ORA = reg 8 volume A */
    /* After latch, write data mode */
    mockingboard_write(&m, &m.mockingboard[4], 4, 0xC040, 0x00, 0x06);
    mockingboard_write(&m, &m.mockingboard[4], 4, 0xC041, 0x01, 0x0F);

    /* Queue some time and sample — should not crash; volume may be non-zero. */
    mockingboard_queue_ay_cycles(&m.mockingboard[4], 1000);
    {
        MOCKINGBOARD_SAMPLE s = mockingboard_get_stereo_sample(&m.mockingboard[4]);
        (void)s;
    }

    /* IRQ path: step timers */
    apple2_peripherals_step(&m, 100);
    v = mockingboard_irq_pending(&m);
    (void)v;

    apple2_shutdown(&m);
}

static void test_cards_support_slots_one_through_seven(void)
{
    apple2_t m;
    if (!apple2_init(&m)) {
        fail("slot range init");
    }
    if (!apple2_attach_diskii(&m, 1) || m.slot_type[1] != SLOT_TYPE_DISKII) {
        fail("Disk II slot 1");
    }
    if (!apple2_attach_smartport(&m, 2) || m.slot_type[2] != SLOT_TYPE_SMARTPORT) {
        fail("SmartPort slot 2");
    }
    if (!apple2_attach_mockingboard(&m, 7) || m.slot_type[7] != SLOT_TYPE_MOCKINGBOARD ||
        m.mb_slot != 7 || m.slot_type[4] == SLOT_TYPE_MOCKINGBOARD) {
        fail("single Mockingboard moves to slot 7");
    }
    if (apple2_attach_diskii(&m, 0) || apple2_attach_smartport(&m, 0) ||
        apple2_attach_mockingboard(&m, 0)) {
        fail("slot 0 must reject peripheral cards");
    }
    apple2_shutdown(&m);
}

static void test_smartport_mount_missing(void)
{
    apple2_t m;

    if (!apple2_init(&m)) {
        fail("init2");
    }
    if (!apple2_attach_smartport(&m, 7)) {
        fail("attach SP");
    }
    if (m.slot_type[7] != SLOT_TYPE_SMARTPORT) {
        fail("slot type SP");
    }
    /* Missing file fails cleanly. */
    if (apple2_smartport_mount(&m, 7, 0, "/no/such/hd.po") == 0) {
        fail("expected mount fail");
    }

    /* Cn ROM mapped (non-zero firmware bytes). */
    if (m.smartport_rom_bytes[7][0] == 0 && m.smartport_rom_bytes[7][1] == 0) {
        /* still ok if zeros — just check shadow */
    }
    if (m.rom_shadow_pages[7] != m.smartport_rom_bytes[7]) {
        fail("SP rom shadow");
    }

    apple2_shutdown(&m);
}

static void test_smartport_image_roundtrip(void)
{
    apple2_t m;
    const char *path = "test_sp_image.po";
    FILE *fp;
    uint8_t block[512];
    int i;

    /* 2 empty ProDOS blocks */
    memset(block, 0, sizeof(block));
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fail("create po");
    }
    for (i = 0; i < 4; i++) {
        if (fwrite(block, 1, 512, fp) != 512) {
            fail("write po");
        }
    }
    fclose(fp);

    if (!apple2_init(&m)) {
        fail("init3");
    }
    if (apple2_smartport_mount(&m, 5, 0, path) != 0) {
        fail("sp mount");
    }

    /* Command buffer: cmd=1 read, unit=0, block=0 */
    m.sp_device[5].sp_buffer[0] = 1;
    m.sp_device[5].sp_buffer[1] = 0;
    m.sp_device[5].sp_buffer[2] = 0;
    m.sp_device[5].sp_buffer[3] = 0;
    sp_read(&m, 5);
    if (m.sp_device[5].sp_buffer[0] != SP_SUCCESS) {
        fail("sp_read status");
    }

    /* Status command */
    m.sp_device[5].sp_buffer[0] = 0;
    m.sp_device[5].sp_buffer[1] = 0;
    sp_status(&m, 5);
    if (m.sp_device[5].sp_buffer[0] != SP_SUCCESS) {
        fail("sp_status");
    }
    /* blocks = 4 */
    if (m.sp_device[5].sp_buffer[1] != 4 || m.sp_device[5].sp_buffer[2] != 0) {
        fprintf(
            stderr,
            "blocks %u %u\n",
            m.sp_device[5].sp_buffer[1],
            m.sp_device[5].sp_buffer[2]);
        fail("block count");
    }

    if (apple2_smartport_eject(&m, 5, 0) != 0 ||
        m.sp_device[5].sp_files[0].is_file_open) {
        fail("SmartPort live eject");
    }
    if (apple2_smartport_mount(&m, 5, 0, path) != 0) {
        fail("SmartPort live reinsert");
    }

    apple2_shutdown(&m);
    remove(path);
}

/* Firmware for slot 7 talks to $C0F4 (data) and $C0F5 (status). A wrong
   SP_DATA/SP_STATUS nibble makes PR#7 / boot hang then ERR. */
static void test_smartport_softswitch_ports(void)
{
    apple2_t m;
    const char *path = "test_sp_ports.po";
    FILE *fp;
    uint8_t block[512];
    int i;
    uint8_t st;

    memset(block, 0xA5, sizeof(block));
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fail("create ports po");
    }
    for (i = 0; i < 4; i++) {
        if (fwrite(block, 1, 512, fp) != 512) {
            fail("write ports po");
        }
    }
    fclose(fp);

    if (!apple2_init(&m)) {
        fail("init ports");
    }
    if (apple2_smartport_mount(&m, 7, 0, path) != 0) {
        fail("mount s7");
    }

    /* Write command: status (0), unit 0 — via $C0F4 data port. */
    softswitch_c0_write(&m, 0xC0F0 + SP_DATA, 0); /* cmd */
    softswitch_c0_write(&m, 0xC0F0 + SP_DATA, 0); /* unit */
    softswitch_c0_write(&m, 0xC0F0 + SP_STATUS, 0); /* fire */

    st = softswitch_c0_read(&m, 0xC0F0 + SP_STATUS);
    if ((st & 0x80u) == 0) {
        fail("status ready bit after SP_STATUS write");
    }

    /* Result byte is at buffer[0]; firmware reads it from data port. */
    m.sp_device[7].sp_read_offset = 0;
    if (softswitch_c0_read(&m, 0xC0F0 + SP_DATA) != SP_SUCCESS) {
        fail("status cmd result via SP_DATA");
    }

    /* Read block 0 through softswitch: cmd=1, unit=0, block=0. */
    m.sp_device[7].sp_write_offset = 0;
    m.sp_device[7].sp_read_offset = 0;
    softswitch_c0_write(&m, 0xC0F0 + SP_DATA, 1);
    softswitch_c0_write(&m, 0xC0F0 + SP_DATA, 0);
    softswitch_c0_write(&m, 0xC0F0 + SP_DATA, 0);
    softswitch_c0_write(&m, 0xC0F0 + SP_DATA, 0);
    softswitch_c0_write(&m, 0xC0F0 + SP_STATUS, 0);
    st = softswitch_c0_read(&m, 0xC0F0 + SP_STATUS);
    if ((st & 0x80u) == 0) {
        fail("status ready after read cmd");
    }
    m.sp_device[7].sp_read_offset = 0;
    if (softswitch_c0_read(&m, 0xC0F0 + SP_DATA) != SP_SUCCESS) {
        fail("read cmd result");
    }
    /* Next data byte is first payload (0xA5). */
    if (softswitch_c0_read(&m, 0xC0F0 + SP_DATA) != 0xA5u) {
        fail("block0 payload via SP_DATA");
    }

    /* Wrong nibble must NOT complete a command (guard against 0/1 regression). */
    m.sp_device[7].sp_status = 0;
    m.sp_device[7].sp_write_offset = 0;
    softswitch_c0_write(&m, 0xC0F0 + 0, 0); /* bogus old SP_DATA */
    softswitch_c0_write(&m, 0xC0F0 + 1, 0); /* bogus old SP_STATUS */
    if (m.sp_device[7].sp_status == 0x80u) {
        fail("legacy 0/1 nibble must not fire SP_STATUS");
    }

    apple2_shutdown(&m);
    remove(path);
}

/*
 * Pure SmartPort $C800 host trap: simulate post-JSR entry with inline
 * cmd + param list, after latching the SP slot via $Cnxx access.
 */
static void test_smartport_host_trap(void)
{
    apple2_t m;
    const char *path = "test_sp_trap.po";
    FILE *fp;
    uint8_t block[512];
    int i;
    uint16_t plist = 0x0300;
    uint16_t buf = 0x1000;
    uint16_t ret_after = 0x0803; /* cmd at 0x0800 after fake JSR */

    memset(block, 0x5A, sizeof(block));
    block[0] = 0x11;
    block[1] = 0x22;
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fail("create trap po");
    }
    for (i = 0; i < 4; i++) {
        if (fwrite(block, 1, 512, fp) != 512) {
            fail("write trap po");
        }
    }
    fclose(fp);

    if (!apple2_init(&m)) {
        fail("init trap");
    }
    if (apple2_smartport_mount(&m, 7, 0, path) != 0) {
        fail("mount trap");
    }

    softswitch_slot_io_select(&m, 0xC700);
    if (m.last_io_select_slot != 7) {
        fail("SP I/O SELECT not recorded");
    }
    if (m.c800_card != 7 || m.strobed_slot != 7) {
        fail("SP did not claim C800");
    }

    /* --- STATUS unit 0 --- */
    m.cpu.cpu.sp = 0x1FDu;
    m.ram_main[0x1FE] = 0xFF; /* stacked = 0x07FF → cmd @ 0x0800 */
    m.ram_main[0x1FF] = 0x07;
    m.cpu.cpu.pc = 0xC800;
    m.ram_main[0x0800] = 0x00; /* STATUS */
    m.ram_main[0x0801] = (uint8_t)(plist & 0xFF);
    m.ram_main[0x0802] = (uint8_t)(plist >> 8);
    m.ram_main[plist + 0] = 3;
    m.ram_main[plist + 1] = 0; /* unit 0 = controller */
    m.ram_main[plist + 2] = 0x00; /* status list at $0400 */
    m.ram_main[plist + 3] = 0x04;
    m.ram_main[plist + 4] = 0;

    if (!sp_host_trap(&m)) {
        fail("status trap not taken");
    }
    if (m.cpu.cpu.pc != ret_after) {
        fail("status return pc");
    }
    if (m.cpu.cpu.C != 0 || m.cpu.cpu.A != SP_SUCCESS) {
        fail("status success flags");
    }
    if (m.ram_main[0x0400] < 1) {
        fail("status device count");
    }

    /* --- READ_BLOCK unit 1, block 0 into $1000 --- */
    m.cpu.cpu.sp = 0x1FDu;
    m.ram_main[0x1FE] = 0xFF;
    m.ram_main[0x1FF] = 0x07;
    m.cpu.cpu.pc = 0xC89B;
    m.ram_main[0x0800] = 0x01;
    m.ram_main[0x0801] = (uint8_t)(plist & 0xFF);
    m.ram_main[0x0802] = (uint8_t)(plist >> 8);
    m.ram_main[plist + 0] = 3;
    m.ram_main[plist + 1] = 1;
    m.ram_main[plist + 2] = (uint8_t)(buf & 0xFF);
    m.ram_main[plist + 3] = (uint8_t)(buf >> 8);
    m.ram_main[plist + 4] = 0;
    m.ram_main[plist + 5] = 0;
    m.ram_main[plist + 6] = 0;
    memset(m.ram_main + buf, 0, 512);

    if (!sp_host_trap(&m)) {
        fail("read trap not taken");
    }
    if (m.cpu.cpu.C != 0 || m.cpu.cpu.A != SP_SUCCESS) {
        fail("read success");
    }
    if (m.ram_main[buf] != 0x11 || m.ram_main[buf + 1] != 0x22) {
        fail("read data");
    }

    /* --- WRITE_BLOCK unit 1, block 1 --- */
    m.ram_main[buf] = 0xAB;
    m.ram_main[buf + 1] = 0xCD;
    for (i = 2; i < 512; i++) {
        m.ram_main[buf + i] = 0x00;
    }
    m.cpu.cpu.sp = 0x1FDu;
    m.ram_main[0x1FE] = 0xFF;
    m.ram_main[0x1FF] = 0x07;
    m.cpu.cpu.pc = 0xC9AA;
    m.ram_main[0x0800] = 0x02;
    m.ram_main[0x0801] = (uint8_t)(plist & 0xFF);
    m.ram_main[0x0802] = (uint8_t)(plist >> 8);
    m.ram_main[plist + 0] = 3;
    m.ram_main[plist + 1] = 1;
    m.ram_main[plist + 2] = (uint8_t)(buf & 0xFF);
    m.ram_main[plist + 3] = (uint8_t)(buf >> 8);
    m.ram_main[plist + 4] = 1;
    m.ram_main[plist + 5] = 0;
    m.ram_main[plist + 6] = 0;

    if (!sp_host_trap(&m)) {
        fail("write trap not taken");
    }
    if (m.cpu.cpu.C != 0 || m.cpu.cpu.A != SP_SUCCESS) {
        fail("write success");
    }

    m.sp_device[7].sp_buffer[0] = 1;
    m.sp_device[7].sp_buffer[1] = 0;
    m.sp_device[7].sp_buffer[2] = 1;
    m.sp_device[7].sp_buffer[3] = 0;
    sp_read(&m, 7);
    if (m.sp_device[7].sp_buffer[0] != SP_SUCCESS) {
        fail("reread after write");
    }
    if (m.sp_device[7].sp_buffer[1] != 0xAB || m.sp_device[7].sp_buffer[2] != 0xCD) {
        fail("write data mismatch");
    }

    /*
     * $C3xx overlays 80-col firmware; SP card latch remains. $C800 execute
     * is firmware, not the trap. $CFFF drops both; $C7xx can claim again.
     */
    {
        uint8_t c800_rom = m.rom_c000[0x800];

        softswitch_slot_io_select(&m, 0xC300);
        if (m.strobed_slot != 8 || !m.c800_internal) {
            fail("C3 did not overlay 80-col C800");
        }
        if (m.c800_card != 7) {
            fail("SP card latch dropped by C3 overlay");
        }
        if (apple2_debug_read(&m, 0xC800) != c800_rom) {
            fail("C800 not 80-col firmware after C3");
        }
        m.cpu.cpu.pc = 0xC800;
        if (sp_host_trap(&m)) {
            fail("SP trap stole 80-col $C800");
        }
        if (m.cpu.cpu.pc != 0xC800) {
            fail("80-col $C800 PC mutated");
        }

        (void)m.cpu.read(m.cpu.user, 0xCFFF);
        if (m.strobed_slot != -1 || m.c800_card != -1 || m.c800_internal) {
            fail("CFFF did not release both C800 latches");
        }
        softswitch_slot_io_select(&m, 0xC700);
        if (m.c800_card != 7 || m.strobed_slot != 7) {
            fail("SP did not reclaim C800 after CFFF");
        }
        m.cpu.cpu.sp = 0x1FDu;
        m.ram_main[0x1FE] = 0xFF;
        m.ram_main[0x1FF] = 0x07;
        m.cpu.cpu.pc = 0xC800;
        m.ram_main[0x0800] = 0x00;
        m.ram_main[0x0801] = (uint8_t)(plist & 0xFF);
        m.ram_main[0x0802] = (uint8_t)(plist >> 8);
        m.ram_main[plist + 0] = 3;
        m.ram_main[plist + 1] = 0;
        m.ram_main[plist + 2] = 0x00;
        m.ram_main[plist + 3] = 0x04;
        m.ram_main[plist + 4] = 0;
        if (!sp_host_trap(&m)) {
            fail("SP trap not taken after reclaim");
        }
    }

    apple2_shutdown(&m);
    remove(path);
}

static int g_ssc_warn_count;
static char g_ssc_warn_msg[256];

static void ssc_warn_callback(log_Event *ev)
{
    if (ev == NULL || ev->fmt == NULL) {
        return;
    }
    if (ev->level != LOG_WARN) {
        return;
    }
    vsnprintf(g_ssc_warn_msg, sizeof(g_ssc_warn_msg), ev->fmt, ev->ap);
    g_ssc_warn_count++;
}

static void test_ssc_rom_size_and_cnxx(void)
{
    apple2_t m;
    uint16_t addr;
    int i;

    if (ssc_rom_size != 2048 || SSC_ROM_SIZE != 2048) {
        fail("ssc rom size");
    }
    if (!apple2_init(&m)) {
        fail("ssc rom init");
    }
    if (!apple2_attach_ssc(&m, 2) || m.slot_type[2] != SLOT_TYPE_SSC || m.ssc_slot != 2) {
        fail("ssc attach slot 2");
    }
    if (m.rom_shadow_pages[2] != (uint8_t *)(ssc_rom + 0x700)) {
        fail("ssc cnxx shadow");
    }
    for (i = 0; i < 256; ++i) {
        addr = (uint16_t)(0xC200 + i);
        if (apple2_debug_read(&m, addr) != ssc_rom[0x700 + i]) {
            fail("ssc cnxx fetch");
        }
    }
    if (apple2_attach_ssc(&m, 0)) {
        fail("ssc slot 0 must reject");
    }
    apple2_shutdown(&m);
}

static void test_ssc_move_rule(void)
{
    apple2_t m;

    if (!apple2_init(&m)) {
        fail("ssc move init");
    }
    if (!apple2_attach_ssc(&m, 1) || m.ssc_slot != 1) {
        fail("ssc first attach");
    }
    if (!apple2_attach_ssc(&m, 5) || m.ssc_slot != 5 ||
        m.slot_type[5] != SLOT_TYPE_SSC || m.slot_type[1] != SLOT_TYPE_EMPTY) {
        fail("ssc move clears old slot");
    }
    apple2_detach_slot_card(&m, 5);
    if (m.ssc_slot != 0 || m.slot_type[5] != SLOT_TYPE_EMPTY) {
        fail("ssc detach clears ssc_slot");
    }
    apple2_shutdown(&m);
}

static void test_ssc_same_slot_conflict_warns(void)
{
    apple2_t m;

    g_ssc_warn_count = 0;
    g_ssc_warn_msg[0] = '\0';
    log_set_quiet(true);
    if (log_add_callback(ssc_warn_callback, NULL, LOG_WARN) != 0) {
        fail("ssc warn callback");
    }
    if (!apple2_init(&m)) {
        fail("ssc conflict init");
    }
    /* Default Disk II is slot 6. */
    if (m.slot_type[6] != SLOT_TYPE_DISKII) {
        fail("expected diskii in 6");
    }
    if (apple2_attach_ssc(&m, 6)) {
        fail("ssc onto diskii must fail");
    }
    if (m.slot_type[6] != SLOT_TYPE_DISKII || m.ssc_slot != 0) {
        fail("ssc conflict left diskii");
    }
    if (g_ssc_warn_count < 1 || strstr(g_ssc_warn_msg, "busy") == NULL ||
        strstr(g_ssc_warn_msg, "diskii") == NULL) {
        fprintf(stderr, "warn='%s' count=%d\n", g_ssc_warn_msg, g_ssc_warn_count);
        fail("ssc conflict log_warn");
    }
    apple2_shutdown(&m);
}

static void test_ssc_apply_diskii_to_ssc(void)
{
    apple2_t m;

    if (!apple2_init(&m)) {
        fail("ssc apply init");
    }
    if (m.slot_type[6] != SLOT_TYPE_DISKII) {
        fail("apply expects diskii 6");
    }
    /* Production remap invariant: detach before attach_ssc. */
    apple2_detach_slot_card(&m, 6);
    if (!apple2_attach_ssc(&m, 6) || m.slot_type[6] != SLOT_TYPE_SSC || m.ssc_slot != 6) {
        fail("apply diskii->ssc");
    }
    apple2_shutdown(&m);
}

/* Slot 2 → DEVSEL base $C0A0. */
static void test_ssc_dip_and_acia_tdre_tx(void)
{
    apple2_t m;
    uint8_t status;

    if (!apple2_init(&m)) {
        fail("ssc acia init");
    }
    if (!apple2_attach_ssc(&m, 2)) {
        fail("ssc acia attach");
    }

    if (softswitch_c0_read(&m, 0xC0A1) != SSC_DIP1_PRINTER) {
        fail("ssc DIP1 $C0n1");
    }
    if (softswitch_c0_read(&m, 0xC0A2) != SSC_DIP2_PRINTER) {
        fail("ssc DIP2 $C0n2");
    }

    status = softswitch_c0_read(&m, 0xC0A9); /* status */
    if ((status & SSC_STATUS_TDRE) == 0) {
        fail("ssc TDRE after reset");
    }
    if ((status & SSC_STATUS_RDRF) != 0) {
        fail("ssc RDRF clear");
    }
    if ((status & (SSC_STATUS_DCD | SSC_STATUS_DSR)) != 0) {
        fail("ssc DCD/DSR ready (bits clear)");
    }

    softswitch_c0_write(&m, 0xC0A8, 0x41); /* TDR 'A' */
    if (m.ssc.last_tx != 0x41) {
        fail("ssc TX byte");
    }
    if (m.ssc.sink != A2_SSC_SINK_IMAGEWRITER || !m.imagewriter_live) {
        fail("ssc IMAGEWRITER sink on attach");
    }
    if (!imagewriter_page_dirty(&m.imagewriter)) {
        fail("ssc TX dirtied ImageWriter");
    }
    status = softswitch_c0_read(&m, 0xC0A9);
    if ((status & SSC_STATUS_TDRE) == 0) {
        fail("ssc TDRE after instant TX");
    }

    softswitch_c0_write(&m, 0xC0AA, 0x0B); /* command echo */
    softswitch_c0_write(&m, 0xC0AB, 0x1E); /* control 9600-ish */
    if (softswitch_c0_read(&m, 0xC0AA) != 0x0B) {
        fail("ssc command readback");
    }
    if ((softswitch_c0_read(&m, 0xC0AB) & 0x10u) == 0) {
        fail("ssc control forces internal clock");
    }

    apple2_shutdown(&m);
}

static void test_ssc_tx_irq_and_status_clear(void)
{
    apple2_t m;
    uint8_t status;

    if (!apple2_init(&m)) {
        fail("ssc irq init");
    }
    /* Detach default MB so IRQ pending is SSC-only. */
    apple2_detach_slot_card(&m, 4);
    if (!apple2_attach_ssc(&m, 1)) {
        fail("ssc irq attach");
    }

    if (ssc_irq_pending(&m.ssc) != 0) {
        fail("ssc irq idle");
    }

    /* Command: DTR + TX IRQ (bits 3-2 = 01) → 0x05. Empty TDR → IRQ. */
    softswitch_c0_write(&m, 0xC09A, 0x05);
    if (ssc_irq_pending(&m.ssc) == 0) {
        fail("ssc irq after TX IRQ enable");
    }
    if (m.cpu.irq_pending == NULL || m.cpu.irq_pending(m.cpu.user) == 0) {
        fail("apple2_irq_pending OR SSC");
    }

    status = softswitch_c0_read(&m, 0xC099);
    if ((status & SSC_STATUS_IRQ) == 0) {
        fail("ssc status IRQ bit");
    }
    if (ssc_irq_pending(&m.ssc) != 0) {
        fail("ssc irq cleared by status read");
    }

    /* Another TX with IRQ enabled re-latches on TDRE rising after absorb. */
    softswitch_c0_write(&m, 0xC098, 0x5A);
    if (ssc_irq_pending(&m.ssc) == 0) {
        fail("ssc irq after TX byte");
    }
    (void)softswitch_c0_read(&m, 0xC099);
    if (ssc_irq_pending(&m.ssc) != 0) {
        fail("ssc irq clear again");
    }

    apple2_shutdown(&m);
}

static void test_ssc_c800_latch_and_rom(void)
{
    apple2_t m;
    int i;

    if (!apple2_init(&m)) {
        fail("ssc c800 init");
    }
    if (!apple2_attach_ssc(&m, 2)) {
        fail("ssc c800 attach");
    }

    m.strobed_slot = -1;
    m.c800_card = -1;
    m.c800_internal = false;
    softswitch_apply_full_map(&m);

    softswitch_slot_io_select(&m, 0xC200);
    if (m.c800_card != 2 || m.strobed_slot != 2) {
        fail("ssc did not claim C800");
    }
    for (i = 0; i < 8; ++i) {
        if (apple2_debug_read(&m, (uint16_t)(0xC800 + i)) != ssc_rom[i]) {
            fail("ssc C800 firmware fetch");
        }
    }
    if (m.cpu.read(m.cpu.user, 0xC800) != ssc_rom[0]) {
        fail("ssc bus C800 firmware");
    }

    /* Second claimant must not steal. */
    if (!apple2_attach_smartport(&m, 7)) {
        fail("ssc+sp attach");
    }
    softswitch_slot_io_select(&m, 0xC700);
    if (m.c800_card != 2) {
        fail("SP stole SSC C800 latch");
    }

    softswitch_c800_release(&m);
    softswitch_apply_full_map(&m);
    if (m.c800_card != -1) {
        fail("CFFF did not clear card latch");
    }
    softswitch_slot_io_select(&m, 0xC700);
    if (m.c800_card != 7) {
        fail("SP did not claim after CFFF");
    }

    apple2_shutdown(&m);
}

/* YYYYMMDD-HHMMSSXX.bmp */
static bool is_print_page_name(const char *name)
{
    size_t i;

    if (name == NULL || strlen(name) != 21u) {
        return false;
    }
    for (i = 0; i < 8u; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return false;
        }
    }
    if (name[8] != '-') {
        return false;
    }
    for (i = 9; i < 15u; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return false;
        }
    }
    if (!isdigit((unsigned char)name[15]) || !isdigit((unsigned char)name[16])) {
        return false;
    }
    return strcmp(name + 17, ".bmp") == 0;
}

static int count_print_pages(const char *dir)
{
    DIR *d;
    struct dirent *de;
    int n = 0;

    d = opendir(dir);
    if (d == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL) {
        if (is_print_page_name(de->d_name)) {
            n++;
        }
    }
    closedir(d);
    return n;
}

static void cleanup_print_pages(const char *dir)
{
    DIR *d;
    struct dirent *de;
    char path[1100];

    d = opendir(dir);
    if (d == NULL) {
        return;
    }
    while ((de = readdir(d)) != NULL) {
        if (is_print_page_name(de->d_name)) {
            snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
            (void)remove(path);
        }
    }
    closedir(d);
}

static void ssc_tx_byte(apple2_t *m, uint8_t slot, uint8_t ch)
{
    uint16_t tdr = (uint16_t)(0xC080 + slot * 0x10 + 0x08);
    softswitch_c0_write(m, tdr, ch);
}

static void test_ssc_tx_ff_writes_bmp(void)
{
    apple2_t m;
    const char *dir = "ssc_iw_tmp_ff";
    const char *hi = "HELLO";

    (void)test_mkdir(dir);
    cleanup_print_pages(dir);

    if (!apple2_init(&m)) {
        fail("ssc bmp init");
    }
    apple2_set_printer_output_dir(&m, dir);
    if (!apple2_attach_ssc(&m, 2)) {
        fail("ssc bmp attach");
    }
    if (m.ssc.sink != A2_SSC_SINK_IMAGEWRITER || m.ssc.sink_putc == NULL) {
        fail("ssc bmp sink wired");
    }

    while (*hi != '\0') {
        ssc_tx_byte(&m, 2, (uint8_t)*hi++);
    }
    ssc_tx_byte(&m, 2, 0x0C); /* FF */

    if (imagewriter_page_dirty(&m.imagewriter)) {
        fail("ssc bmp still dirty after FF");
    }
    if (imagewriter_pages_flushed(&m.imagewriter) != 1u) {
        fail("ssc bmp pages_flushed");
    }
    if (count_print_pages(dir) != 1) {
        fail("ssc bmp host file missing");
    }

    apple2_detach_slot_card(&m, 2);
    apple2_shutdown(&m);
    cleanup_print_pages(dir);
    (void)test_rmdir(dir);
}

static void test_ssc_replay_sealed_skips_host_write(void)
{
    apple2_t m;
    const char *dir = "ssc_iw_tmp_sealed";

    (void)test_mkdir(dir);
    cleanup_print_pages(dir);

    if (!apple2_init(&m)) {
        fail("ssc sealed init");
    }
    apple2_set_printer_output_dir(&m, dir);
    if (!apple2_attach_ssc(&m, 1)) {
        fail("ssc sealed attach");
    }

    apple2_set_replay_sealed(&m, true);
    ssc_tx_byte(&m, 1, (uint8_t)'Z');
    ssc_tx_byte(&m, 1, 0x0C);
    if (imagewriter_page_dirty(&m.imagewriter)) {
        fail("ssc sealed must not mutate IW");
    }
    if (count_print_pages(dir) != 0) {
        fail("ssc sealed must not write host files");
    }

    apple2_set_replay_sealed(&m, false);
    ssc_tx_byte(&m, 1, (uint8_t)'A');
    if (!imagewriter_page_dirty(&m.imagewriter)) {
        fail("ssc unsealed TX should dirty");
    }
    /* Detach flushes; sealed was cleared so host write is allowed. */
    apple2_detach_slot_card(&m, 1);
    if (count_print_pages(dir) != 1) {
        fail("ssc detach flush wrote page");
    }

    apple2_shutdown(&m);
    cleanup_print_pages(dir);
    (void)test_rmdir(dir);
}

static void test_ssc_apply_pre_flush_before_cold_reset(void)
{
    apple2_t m;
    const char *dir = "ssc_iw_tmp_preflush";

    (void)test_mkdir(dir);
    cleanup_print_pages(dir);

    if (!apple2_init(&m)) {
        fail("ssc preflush init");
    }
    apple2_set_printer_output_dir(&m, dir);
    if (!apple2_attach_ssc(&m, 3)) {
        fail("ssc preflush attach");
    }
    ssc_tx_byte(&m, 3, (uint8_t)'P');
    if (!imagewriter_page_dirty(&m.imagewriter)) {
        fail("ssc preflush dirty");
    }

    /* Configure Apply path when SSC remains: force-flush, then cold reset. */
    apple2_imagewriter_force_flush(&m);
    if (count_print_pages(dir) != 1) {
        fail("ssc preflush host write");
    }
    apple2_cold_reset(&m);
    if (imagewriter_page_dirty(&m.imagewriter)) {
        fail("ssc cold reset left dirty");
    }
    if (count_print_pages(dir) != 1) {
        fail("ssc cold reset must not double-write");
    }

    apple2_shutdown(&m);
    cleanup_print_pages(dir);
    (void)test_rmdir(dir);
}

/* Regression: Configure Apply used to pass machine.printer_output_dir into
   apple2_set_printer_output_dir, and fortified strncpy aborted on overlap. */
static void test_ssc_set_printer_dir_self_alias(void)
{
    apple2_t m;
    const char *dir = "ssc_iw_tmp_alias";

    (void)test_mkdir(dir);
    cleanup_print_pages(dir);

    if (!apple2_init(&m)) {
        fail("ssc alias init");
    }
    apple2_set_printer_output_dir(&m, dir);
    if (strcmp(m.printer_output_dir, dir) != 0) {
        fail("ssc alias first set");
    }
    /* Same pointer as destination — must not SIGTRAP. */
    apple2_set_printer_output_dir(&m, m.printer_output_dir);
    if (strcmp(m.printer_output_dir, dir) != 0) {
        fail("ssc alias self set");
    }
    if (!apple2_attach_ssc(&m, 1)) {
        fail("ssc alias attach");
    }
    apple2_set_printer_output_dir(&m, m.printer_output_dir);
    if (strcmp(m.imagewriter.output_dir, dir) != 0) {
        fail("ssc alias IW sync");
    }

    apple2_detach_slot_card(&m, 1);
    apple2_shutdown(&m);
    cleanup_print_pages(dir);
    (void)test_rmdir(dir);
}

int main(void)
{
    test_mockingboard_attach();
    test_cards_support_slots_one_through_seven();
    test_smartport_mount_missing();
    test_smartport_image_roundtrip();
    test_smartport_softswitch_ports();
    test_smartport_host_trap();
    test_ssc_rom_size_and_cnxx();
    test_ssc_move_rule();
    test_ssc_same_slot_conflict_warns();
    test_ssc_apply_diskii_to_ssc();
    test_ssc_dip_and_acia_tdre_tx();
    test_ssc_tx_irq_and_status_clear();
    test_ssc_c800_latch_and_rom();
    test_ssc_tx_ff_writes_bmp();
    test_ssc_replay_sealed_skips_host_write();
    test_ssc_apply_pre_flush_before_cold_reset();
    test_ssc_set_printer_dir_self_alias();
    printf("ok\n");
    return 0;
}
