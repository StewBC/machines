#include "apple2.h"
#include "apple2_snapshot.h"
#include "memview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static uint8_t *save_machine(apple2_t *m, size_t *out_size)
{
    size_t size;
    size_t written;
    uint8_t *buf;

    expect_true("flush", apple2_snapshot_flush_media(m));
    size = apple2_snapshot_size(m);
    expect_true("size>0", size > 0);
    buf = (uint8_t *)malloc(size);
    expect_true("malloc", buf != NULL);
    written = apple2_snapshot_save(m, buf, size);
    expect_true("save", written == size);
    *out_size = written;
    return buf;
}

static void test_round_trip_basic(void)
{
    apple2_t m;
    apple2_t m2;
    uint8_t *buf = NULL;
    size_t written;
    uint8_t marker[4] = { 0xCA, 0xFE, 0xBA, 0xBE };
    uint16_t pc_saved;
    uint64_t cycles_saved;
    uint32_t flags_saved;
    uint16_t line_saved;
    uint16_t h_saved;
    uint32_t prng_saved;

    expect_true("init", apple2_init(&m));
    apple2_debug_write(&m, 0x0400, marker[0]);
    apple2_debug_write(&m, 0x0401, marker[1]);
    apple2_debug_write(&m, 0x0402, marker[2]);
    apple2_debug_write(&m, 0x0403, marker[3]);
    (void)apple2_step_instruction(&m);
    (void)apple2_step_instruction(&m);
    (void)apple2_step_instruction(&m);

    pc_saved = m.cpu.cpu.pc;
    cycles_saved = m.cpu.cpu.cycles;
    flags_saved = m.state_flags;
    line_saved = m.video.line;
    h_saved = m.video.cycle_in_line;
    (void)apple2_rand_u32(&m);
    prng_saved = m.prng;

    buf = save_machine(&m, &written);

    expect_true("init2", apple2_init(&m2));
    m2.cpu.cpu.pc = 0x1234;
    m2.cpu.cpu.cycles = 99999;
    apple2_debug_write(&m2, 0x0400, 0x00);

    expect_true("load", apple2_snapshot_load(&m2, buf, written));
    if (m2.cpu.cpu.pc != pc_saved) {
        fprintf(stderr, "FAIL: pc %04x != %04x\n", m2.cpu.cpu.pc, pc_saved);
        exit(1);
    }
    if (m2.cpu.cpu.cycles != cycles_saved) {
        fprintf(
            stderr,
            "FAIL: cycles %llu != %llu\n",
            (unsigned long long)m2.cpu.cpu.cycles,
            (unsigned long long)cycles_saved);
        exit(1);
    }
    if (m2.state_flags != flags_saved) {
        fprintf(stderr, "FAIL: flags %08x != %08x\n", m2.state_flags, flags_saved);
        exit(1);
    }
    if (m2.video.line != line_saved || m2.video.cycle_in_line != h_saved) {
        fail("beam not restored");
    }
    if (m2.prng != prng_saved) {
        fprintf(stderr, "FAIL: prng %08x != %08x\n", m2.prng, prng_saved);
        exit(1);
    }
    if (apple2_debug_read(&m2, 0x0400) != marker[0] ||
        apple2_debug_read(&m2, 0x0401) != marker[1] ||
        apple2_debug_read(&m2, 0x0402) != marker[2] ||
        apple2_debug_read(&m2, 0x0403) != marker[3]) {
        fail("RAM marker");
    }

    buf[0] ^= 0xFFu;
    expect_true("bad magic fails", !apple2_snapshot_load(&m2, buf, written));

    free(buf);
    apple2_shutdown(&m);
    apple2_shutdown(&m2);
}

static void test_iie_always_full_ram(void)
{
    apple2_t m;
    apple2_t plus;
    size_t iie_size;
    size_t plus_size;
    uint8_t *iie_buf;
    uint8_t *plus_buf;

    expect_true("init iie", apple2_init(&m));
    expect_true("model iie", m.model == APPLE2_MODEL_IIE_ENHANCED);
    iie_buf = save_machine(&m, &iie_size);

    expect_true("init plus", apple2_init(&plus));
    apple2_set_model(&plus, APPLE2_MODEL_II_PLUS);
    plus_buf = save_machine(&plus, &plus_size);

    if (iie_size <= plus_size) {
        fprintf(
            stderr,
            "FAIL: //e size %zu should exceed ][+ %zu\n",
            iie_size,
            plus_size);
        exit(1);
    }
    if (iie_size - plus_size !=
        (APPLE2_RAM_BANK_SIZE + APPLE2_RAM_LC_BANK_SIZE)) {
        fprintf(
            stderr,
            "FAIL: //e vs ][+ delta %zu != %zu\n",
            iie_size - plus_size,
            (size_t)APPLE2_RAM_BANK_SIZE + (size_t)APPLE2_RAM_LC_BANK_SIZE);
        exit(1);
    }

    free(iie_buf);
    free(plus_buf);
    apple2_shutdown(&m);
    apple2_shutdown(&plus);
}

static void test_plus_omits_aux_round_trip(void)
{
    apple2_t m;
    apple2_t m2;
    uint8_t *buf;
    size_t size;
    view_flags_t vf = 0;

    expect_true("init", apple2_init(&m));
    apple2_set_model(&m, APPLE2_MODEL_II_PLUS);
    apple2_debug_write(&m, 0x2000, 0x42);

    /* Poison aux host storage; ][+ save must not retain it. */
    memset(m.ram_main + APPLE2_RAM_BANK_SIZE, 0x5A, 64);
    memset(m.ram_lc + APPLE2_RAM_LC_AUX_OFFSET, 0x5A, 64);

    buf = save_machine(&m, &size);

    expect_true("init2", apple2_init(&m2));
    apple2_set_model(&m2, APPLE2_MODEL_II_PLUS);
    memset(m2.ram_main + APPLE2_RAM_BANK_SIZE, 0xFF, 64);
    expect_true("load", apple2_snapshot_load(&m2, buf, size));

    if (apple2_debug_read(&m2, 0x2000) != 0x42) {
        fail("][+ main");
    }
    vf_set_ram(&vf, A2SEL48K_AUX);
    if (apple2_read_in_view(&m2, vf, 0x2000) != 0x00) {
        fail("][+ omitted aux not baseline");
    }
    if (m2.ram_lc[APPLE2_RAM_LC_AUX_OFFSET] != 0x00) {
        fail("][+ omitted aux LC not baseline");
    }

    free(buf);
    apple2_shutdown(&m);
    apple2_shutdown(&m2);
}

static void test_ssc_slot_round_trip(void)
{
    apple2_t m;
    apple2_t m2;
    uint8_t *buf = NULL;
    size_t written;

    expect_true("ssc init", apple2_init(&m));
    apple2_detach_slot_card(&m, 6);
    expect_true("ssc attach", apple2_attach_ssc(&m, 6));
    expect_true("ssc present", m.slot_type[6] == SLOT_TYPE_SSC && m.ssc_slot == 6);
    buf = save_machine(&m, &written);

    expect_true("ssc init2", apple2_init(&m2));
    expect_true("ssc load", apple2_snapshot_load(&m2, buf, written));
    if (m2.slot_type[6] != SLOT_TYPE_SSC || m2.ssc_slot != 6) {
        fprintf(
            stderr,
            "FAIL: ssc restore type=%d ssc_slot=%u\n",
            (int)m2.slot_type[6],
            (unsigned)m2.ssc_slot);
        exit(1);
    }

    free(buf);
    apple2_shutdown(&m);
    apple2_shutdown(&m2);
}

static void test_v2_full_ram_still_loads(void)
{
    apple2_t m;
    apple2_t m2;
    uint8_t *buf = NULL;
    size_t size;
    view_flags_t vf = 0;

    expect_true("init", apple2_init(&m));
    vf_set_ram(&vf, A2SEL48K_AUX);
    apple2_write_in_view(&m, vf, 0x1234, 0xEE);
    vf_set_d000(&vf, A2SELD000_LC_B1);
    apple2_write_in_view(&m, vf, 0xE000, 0xCD);
    buf = save_machine(&m, &size);

    /* Downgrade version word to 2; //e payload is still the full layout. */
    buf[4] = 2;
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;

    expect_true("init2", apple2_init(&m2));
    expect_true("load v2-shaped", apple2_snapshot_load(&m2, buf, size));
    vf = 0;
    vf_set_ram(&vf, A2SEL48K_AUX);
    if (apple2_read_in_view(&m2, vf, 0x1234) != 0xEE) {
        fail("v2 aux bank");
    }
    vf_set_d000(&vf, A2SELD000_LC_B1);
    if (apple2_read_in_view(&m2, vf, 0xE000) != 0xCD) {
        fail("v2 aux LC");
    }

    free(buf);
    apple2_shutdown(&m);
    apple2_shutdown(&m2);
}

int main(void)
{
    test_round_trip_basic();
    test_iie_always_full_ram();
    test_plus_omits_aux_round_trip();
    test_ssc_slot_round_trip();
    test_v2_full_ram_still_loads();
    printf("ok\n");
    return 0;
}
