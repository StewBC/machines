#include "c64.h"
#include "c64_hostfs.h"
#include "c64_rom.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define TEST_MKDIR(path) _mkdir(path)
#define TEST_RMDIR(path) _rmdir(path)
#define TEST_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
#define TEST_MKDIR(path) mkdir((path), 0755)
#define TEST_RMDIR(path) rmdir(path)
#define TEST_ISDIR(mode) S_ISDIR(mode)
#endif

enum {
    TEST_PATH_MAX = 512,
    TEST_RETURN_ADDRESS = 0x1233,
    TEST_FILENAME_BUFFER = 0x0200,
    TEST_RESET_VECTOR = 0xe000,
    ZP_LDTND = 0x98,
    ZP_DFLTN = 0x99,
    ZP_DFLTO = 0x9a,
    RAM_LAT = 0x0259,
    RAM_FAT = 0x0263,
    RAM_SAT = 0x026d
};

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, int cond)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        exit(1);
    }
}

static void remove_tree(const char *path)
{
    DIR *dir;
    struct dirent *de;
    char child[TEST_PATH_MAX];

    if (path == NULL || path[0] == '\0') {
        return;
    }
    dir = opendir(path);
    if (dir == NULL) {
        (void)remove(path);
        return;
    }
    while ((de = readdir(dir)) != NULL) {
        struct stat st;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if ((size_t)snprintf(
                child, sizeof(child), "%s/%s", path, de->d_name) >= sizeof(child)) {
            closedir(dir);
            fail("remove_tree path too long");
        }
        if (stat(child, &st) != 0) {
            continue;
        }
        if (TEST_ISDIR(st.st_mode)) {
            remove_tree(child);
        } else {
            (void)remove(child);
        }
    }
    closedir(dir);
    (void)TEST_RMDIR(path);
}

static void make_tmpdir(char *out, size_t out_size)
{
    const char *base;
    char tmpl[TEST_PATH_MAX];

    base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    if ((size_t)snprintf(
            tmpl, sizeof(tmpl), "%s/c64m-printer-XXXXXX", base) >= sizeof(tmpl)) {
        fail("tmpdir template too long");
    }
#if defined(_WIN32)
    {
        static int seq;
        for (;;) {
            if ((size_t)snprintf(
                    out, out_size, "%s/c64m-printer-%d", base, ++seq) >= out_size) {
                fail("tmpdir path too long");
            }
            if (TEST_MKDIR(out) == 0) {
                return;
            }
            if (errno != EEXIST) {
                fail("mkdir tmpdir");
            }
            if (seq > 100000) {
                fail("mkdir tmpdir exhausted");
            }
        }
    }
#else
    if (mkdtemp(tmpl) == NULL) {
        fail("mkdtemp tmpdir");
    }
    if (strlen(tmpl) + 1u > out_size) {
        remove_tree(tmpl);
        fail("tmpdir path too long for caller buffer");
    }
    snprintf(out, out_size, "%s", tmpl);
#endif
}

static void reset_machine(c64_t *machine)
{
    c64_rom_set roms;
    char error[256];

    c64_rom_set_init(&roms);
    roms.has_basic = true;
    roms.has_kernal = true;
    roms.has_character = true;
    memset(roms.basic, 0xea, sizeof(roms.basic));
    memset(roms.kernal, 0xea, sizeof(roms.kernal));
    memset(roms.character, 0, sizeof(roms.character));
    roms.kernal[0x1ffc] = (uint8_t)(TEST_RESET_VECTOR & 0xff);
    roms.kernal[0x1ffd] = (uint8_t)(TEST_RESET_VECTOR >> 8);
    c64_init(machine);
    expect_true("install ROMs", c64_install_roms(machine, &roms, error, sizeof(error)));
    expect_true("reset machine", c64_reset(machine, error, sizeof(error)));
}

static void enable_printer(c64_t *machine, const char *dir)
{
    c64_printer_set_output_dir(&machine->printer, dir);
    c64_printer_set_format_bmp(&machine->printer);
    c64_printer_set_enabled(&machine->printer, true);
}

static void expect_success_return(const c64_t *machine)
{
    expect_true("carry clear", (machine->cpu.cpu.flags & 0x01u) == 0);
    expect_true("status clear", machine->bus.ram[0x90] == 0);
    expect_true(
        "returned",
        machine->cpu.cpu.pc == (uint16_t)(TEST_RETURN_ADDRESS + 1u));
}

static void push_return(c64_t *machine)
{
    machine->cpu.cpu.sp = 0x01fd;
    machine->bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    machine->bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
}

static void setup_open_call(
    c64_t *machine, const char *name, uint8_t la, uint8_t device, uint8_t sa)
{
    size_t length = strlen(name);
    size_t i;

    machine->cpu.cpu.pc = 0xffc0;
    push_return(machine);
    machine->cpu.cpu.flags |= 0x01;
    machine->bus.ram[0xb8] = la;
    machine->bus.ram[0xba] = device;
    machine->bus.ram[0xb9] = sa;
    machine->bus.ram[0xb7] = (uint8_t)length;
    machine->bus.ram[0xbb] = (uint8_t)(TEST_FILENAME_BUFFER & 0xff);
    machine->bus.ram[0xbc] = (uint8_t)(TEST_FILENAME_BUFFER >> 8);
    for (i = 0; i < length; ++i) {
        machine->bus.ram[TEST_FILENAME_BUFFER + i] = (uint8_t)name[i];
    }
}

static void setup_close_call(c64_t *machine, uint8_t la)
{
    machine->cpu.cpu.pc = 0xffc3;
    push_return(machine);
    machine->cpu.cpu.X = la;
    machine->cpu.cpu.flags |= 0x01;
}

static void setup_chkout_call(c64_t *machine, uint8_t la)
{
    machine->cpu.cpu.pc = 0xffc9;
    push_return(machine);
    machine->cpu.cpu.X = la;
    machine->cpu.cpu.flags |= 0x01;
}

static void setup_chrout_call(c64_t *machine, uint8_t byte)
{
    machine->cpu.cpu.pc = 0xffd2;
    push_return(machine);
    machine->cpu.cpu.A = byte;
    machine->cpu.cpu.flags |= 0x01;
}

static void setup_clall_call(c64_t *machine)
{
    machine->cpu.cpu.pc = 0xffe7;
    push_return(machine);
    machine->cpu.cpu.flags |= 0x01;
}

static void step_ok(c64_t *machine, const char *label)
{
    char error[128];
    expect_true(label, c64_step_instruction(machine, error, sizeof(error)));
}

static void lat_add_manual(c64_t *machine, uint8_t la, uint8_t fa, uint8_t sa)
{
    uint8_t n = machine->bus.ram[ZP_LDTND];
    expect_true("lat room", n < 10u);
    machine->bus.ram[RAM_LAT + n] = la;
    machine->bus.ram[RAM_FAT + n] = fa;
    machine->bus.ram[RAM_SAT + n] = sa;
    machine->bus.ram[ZP_LDTND] = (uint8_t)(n + 1u);
}

static int lat_find(const c64_t *machine, uint8_t la)
{
    uint8_t n = machine->bus.ram[ZP_LDTND];
    uint8_t i;
    for (i = 0; i < n; i++) {
        if (machine->bus.ram[RAM_LAT + i] == la) {
            return (int)i;
        }
    }
    return -1;
}

/* YYYYMMDD-HHMMSSXX.bmp */
static bool is_print_page_name(const char *name)
{
    size_t i;

    if (name == NULL || strlen(name) != 21u) {
        return false;
    }
    for (i = 0; i < 8u; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    if (name[8] != '-') {
        return false;
    }
    for (i = 9; i < 15u; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    if (name[15] < '0' || name[15] > '9' || name[16] < '0' || name[16] > '9') {
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

static void open_printer(c64_t *machine, uint8_t la, uint8_t sa)
{
    setup_open_call(machine, "", la, 4, sa);
    step_ok(machine, "open printer");
    expect_success_return(machine);
}

static void open_hostfs_seq_write(c64_t *machine, uint8_t la, uint8_t device, const char *name)
{
    setup_open_call(machine, name, la, device, 2);
    step_ok(machine, "open hostfs seq");
    expect_success_return(machine);
}

/* BASIC bar: OPEN 4,4 / CHKOUT / CHROUT / CLOSE → page file. */
static void test_printer_basic_open_print_close(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    enable_printer(&c64, dir);

    open_printer(&c64, 4, 0);
    expect_true("lat has la4", lat_find(&c64, 4) >= 0);
    expect_true("fat is device 4", c64.bus.ram[RAM_FAT + lat_find(&c64, 4)] == 4);

    setup_chkout_call(&c64, 4);
    step_ok(&c64, "chkout");
    expect_success_return(&c64);
    expect_true("dflto=4", c64.bus.ram[ZP_DFLTO] == 4);

    setup_chrout_call(&c64, (uint8_t)'H');
    step_ok(&c64, "chrout H");
    expect_success_return(&c64);
    expect_true("dirty after chrout", c64_printer_page_dirty(&c64.printer));

    setup_close_call(&c64, 4);
    step_ok(&c64, "close");
    expect_success_return(&c64);
    expect_true("lat cleared", c64.bus.ram[ZP_LDTND] == 0);
    expect_true("dflto screen", c64.bus.ram[ZP_DFLTO] == 3);
    expect_true("flushed", c64_printer_pages_flushed(&c64.printer) == 1u);
    expect_true("clean", !c64_printer_page_dirty(&c64.printer));
    expect_true("page file", count_print_pages(dir) == 1);

    remove_tree(dir);
    printf("PASS: test_printer_basic_open_print_close\n");
}

static void test_printer_disabled_does_not_claim(void)
{
    static c64_t c64;
    char error[128];

    reset_machine(&c64);
    expect_true("disabled", !c64.printer.enabled);

    setup_open_call(&c64, "", 4, 4, 0);
    expect_true("step open", c64_step_instruction(&c64, error, sizeof(error)));
    /* NOP ROM: no trap RTS, PC advances past OPEN entry. */
    expect_true("no rts claim", c64.cpu.cpu.pc != (uint16_t)(TEST_RETURN_ADDRESS + 1u));
    expect_true("lat empty", c64.bus.ram[ZP_LDTND] == 0);

    printf("PASS: test_printer_disabled_does_not_claim\n");
}

static void test_printer_non_device_does_not_claim(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];
    char error[128];
    uint8_t ldtnd_before;

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    enable_printer(&c64, dir);

    /* Device 5 ≠ printer.device (4): printer trap must not claim. */
    setup_open_call(&c64, "", 5, 5, 0);
    ldtnd_before = c64.bus.ram[ZP_LDTND];
    expect_true("step open 5", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("no rts for FA 5", c64.cpu.cpu.pc != (uint16_t)(TEST_RETURN_ADDRESS + 1u));
    expect_true("lat unchanged", c64.bus.ram[ZP_LDTND] == ldtnd_before);

    /* Device 8 with no HostFS: neither printer nor HostFS claims. */
    setup_open_call(&c64, "X,S,W", 2, 8, 2);
    ldtnd_before = c64.bus.ram[ZP_LDTND];
    expect_true("step open 8", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("no rts for FA 8", c64.cpu.cpu.pc != (uint16_t)(TEST_RETURN_ADDRESS + 1u));
    expect_true("lat still empty", c64.bus.ram[ZP_LDTND] == ldtnd_before);

    /* Device 8 with HostFS mounted still reaches HostFS after printer check. */
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 8, dir, true) == C64_DRIVE_STATUS_OK);
    open_hostfs_seq_write(&c64, 2, 8, "E,S,W");
    expect_true("hostfs claimed FA 8", lat_find(&c64, 2) >= 0);
    expect_true("fat is 8", c64.bus.ram[RAM_FAT + lat_find(&c64, 2)] == 8);

    remove_tree(dir);
    printf("PASS: test_printer_non_device_does_not_claim\n");
}

static void clall_and_expect_empty(c64_t *machine, const char *label)
{
    setup_clall_call(machine);
    step_ok(machine, label);
    expect_success_return(machine);
    expect_true("ldtnd 0", machine->bus.ram[ZP_LDTND] == 0);
    expect_true("dfltn 0", machine->bus.ram[ZP_DFLTN] == 0);
    expect_true("dflto 3", machine->bus.ram[ZP_DFLTO] == 3);
}

static void test_printer_chkin_chrin_not_claimed(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];
    char error[128];

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    enable_printer(&c64, dir);
    open_printer(&c64, 4, 0);

    c64.cpu.cpu.pc = 0xffc6;
    push_return(&c64);
    c64.cpu.cpu.X = 4;
    expect_true("chkin step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("chkin no claim", c64.cpu.cpu.pc != (uint16_t)(TEST_RETURN_ADDRESS + 1u));

    c64.bus.ram[ZP_DFLTN] = 4;
    c64.cpu.cpu.pc = 0xffcf;
    push_return(&c64);
    expect_true("chrin step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("chrin no claim", c64.cpu.cpu.pc != (uint16_t)(TEST_RETURN_ADDRESS + 1u));

    remove_tree(dir);
    printf("PASS: test_printer_chkin_chrin_not_claimed\n");
}

static void test_printer_sealed_skips_host_write(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    enable_printer(&c64, dir);
    c64_set_replay_sealed(&c64, true);

    open_printer(&c64, 4, 0);
    setup_chkout_call(&c64, 4);
    step_ok(&c64, "chkout sealed");
    expect_success_return(&c64);
    setup_chrout_call(&c64, (uint8_t)'X');
    step_ok(&c64, "chrout sealed");
    expect_success_return(&c64);
    expect_true("no dirty sealed", !c64_printer_page_dirty(&c64.printer));

    setup_close_call(&c64, 4);
    step_ok(&c64, "close sealed");
    expect_success_return(&c64);
    expect_true("no page sealed", count_print_pages(dir) == 0);

    /* Dirty page then sealed CLALL must not write a host page. */
    c64_set_replay_sealed(&c64, false);
    open_printer(&c64, 4, 0);
    setup_chkout_call(&c64, 4);
    step_ok(&c64, "chkout dirty");
    setup_chrout_call(&c64, (uint8_t)'A');
    step_ok(&c64, "chrout dirty");
    expect_true("dirty before sealed clall", c64_printer_page_dirty(&c64.printer));
    c64_set_replay_sealed(&c64, true);
    clall_and_expect_empty(&c64, "clall sealed");
    expect_true("still dirty sealed clall", c64_printer_page_dirty(&c64.printer));
    expect_true("no flush sealed clall", c64_printer_pages_flushed(&c64.printer) == 0u);
    expect_true("no page sealed clall", count_print_pages(dir) == 0);

    remove_tree(dir);
    printf("PASS: test_printer_sealed_skips_host_write\n");
}

static void test_clall_printer_only(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    enable_printer(&c64, dir);

    open_printer(&c64, 4, 0);
    setup_chkout_call(&c64, 4);
    step_ok(&c64, "chkout");
    setup_chrout_call(&c64, (uint8_t)'A');
    step_ok(&c64, "chrout");
    expect_true("dirty", c64_printer_page_dirty(&c64.printer));

    clall_and_expect_empty(&c64, "clall printer-only");
    expect_true("flushed", c64_printer_pages_flushed(&c64.printer) == 1u);

    remove_tree(dir);
    printf("PASS: test_clall_printer_only\n");
}

static void test_clall_hostfs_only(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 8, dir, true) == C64_DRIVE_STATUS_OK);

    open_hostfs_seq_write(&c64, 2, 8, "OUT,S,W");
    expect_true("lat has hostfs", lat_find(&c64, 2) >= 0);
    clall_and_expect_empty(&c64, "clall hostfs-only");

    remove_tree(dir);
    printf("PASS: test_clall_hostfs_only\n");
}

static void test_clall_hostfs_then_printer(void)
{
    static c64_t c64;
    char host_dir[TEST_PATH_MAX];
    char print_dir[TEST_PATH_MAX];

    make_tmpdir(host_dir, sizeof(host_dir));
    make_tmpdir(print_dir, sizeof(print_dir));
    reset_machine(&c64);
    enable_printer(&c64, print_dir);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 8, host_dir, true) == C64_DRIVE_STATUS_OK);

    open_hostfs_seq_write(&c64, 2, 8, "A,S,W");
    open_printer(&c64, 4, 0);
    expect_true("order hostfs first", c64.bus.ram[RAM_LAT] == 2);
    expect_true("order printer second", c64.bus.ram[RAM_LAT + 1] == 4);

    clall_and_expect_empty(&c64, "clall [HostFS, printer]");

    remove_tree(host_dir);
    remove_tree(print_dir);
    printf("PASS: test_clall_hostfs_then_printer\n");
}

static void test_clall_printer_then_hostfs(void)
{
    static c64_t c64;
    char host_dir[TEST_PATH_MAX];
    char print_dir[TEST_PATH_MAX];

    make_tmpdir(host_dir, sizeof(host_dir));
    make_tmpdir(print_dir, sizeof(print_dir));
    reset_machine(&c64);
    enable_printer(&c64, print_dir);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 8, host_dir, true) == C64_DRIVE_STATUS_OK);

    open_printer(&c64, 4, 0);
    open_hostfs_seq_write(&c64, 2, 8, "B,S,W");
    expect_true("order printer first", c64.bus.ram[RAM_LAT] == 4);
    expect_true("order hostfs second", c64.bus.ram[RAM_LAT + 1] == 2);

    clall_and_expect_empty(&c64, "clall [printer, HostFS]");

    remove_tree(host_dir);
    remove_tree(print_dir);
    printf("PASS: test_clall_printer_then_hostfs\n");
}

static void test_clall_printer_hostfs_printer(void)
{
    static c64_t c64;
    char host_dir[TEST_PATH_MAX];
    char print_dir[TEST_PATH_MAX];

    make_tmpdir(host_dir, sizeof(host_dir));
    make_tmpdir(print_dir, sizeof(print_dir));
    reset_machine(&c64);
    enable_printer(&c64, print_dir);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 8, host_dir, true) == C64_DRIVE_STATUS_OK);

    open_printer(&c64, 4, 0);
    open_hostfs_seq_write(&c64, 2, 8, "C,S,W");
    open_printer(&c64, 5, 7);
    expect_true("lat count 3", c64.bus.ram[ZP_LDTND] == 3);
    expect_true("la0 printer", c64.bus.ram[RAM_LAT] == 4);
    expect_true("la1 hostfs", c64.bus.ram[RAM_LAT + 1] == 2);
    expect_true("la2 printer", c64.bus.ram[RAM_LAT + 2] == 5);

    clall_and_expect_empty(&c64, "clall [printer, HostFS, printer]");

    remove_tree(host_dir);
    remove_tree(print_dir);
    printf("PASS: test_clall_printer_hostfs_printer\n");
}

static void test_clall_mixed_virtual_and_screen(void)
{
    static c64_t c64;
    char dir[TEST_PATH_MAX];
    char error[128];
    uint16_t pc_after;

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    enable_printer(&c64, dir);

    open_printer(&c64, 4, 0);
    /* Foreign screen LA — leave for real KERNAL after virtual pass. */
    lat_add_manual(&c64, 3, 3, 0);
    c64.bus.ram[ZP_DFLTO] = 4;

    setup_chkout_call(&c64, 4);
    step_ok(&c64, "chkout before clall");
    setup_chrout_call(&c64, (uint8_t)'Z');
    step_ok(&c64, "chrout before clall");

    setup_clall_call(&c64);
    expect_true("clall step", c64_step_instruction(&c64, error, sizeof(error)));
    pc_after = c64.cpu.cpu.pc;
    /* Trap closed printer, left screen LA, returned false → NOP executed. */
    expect_true("no rts", pc_after != (uint16_t)(TEST_RETURN_ADDRESS + 1u));
    expect_true("screen la remains", lat_find(&c64, 3) >= 0);
    expect_true("printer la gone", lat_find(&c64, 4) < 0);
    expect_true("one foreign left", c64.bus.ram[ZP_LDTND] == 1);
    expect_true("dflto reset", c64.bus.ram[ZP_DFLTO] == 3);
    expect_true("flushed printer", c64_printer_pages_flushed(&c64.printer) == 1u);

    /* Simulate real KERNAL CLALL finishing foreign LAs. */
    c64.bus.ram[ZP_LDTND] = 0;
    c64.bus.ram[ZP_DFLTN] = 0;
    c64.bus.ram[ZP_DFLTO] = 3;
    expect_true("foreign gone after guest clall", c64.bus.ram[ZP_LDTND] == 0);

    remove_tree(dir);
    printf("PASS: test_clall_mixed_virtual_and_screen\n");
}

static void test_clall_foreign_at_head_with_hostfs(void)
{
    static c64_t c64;
    char host_dir[TEST_PATH_MAX];
    char error[128];

    make_tmpdir(host_dir, sizeof(host_dir));
    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 9, host_dir, true) == C64_DRIVE_STATUS_OK);

    /* Screen at head must survive; HostFS behind it still closes (no FA delete). */
    lat_add_manual(&c64, 3, 3, 0);
    open_hostfs_seq_write(&c64, 2, 9, "D,S,W");
    expect_true("screen head", c64.bus.ram[RAM_LAT] == 3);
    expect_true("hostfs second", c64.bus.ram[RAM_LAT + 1] == 2);

    setup_clall_call(&c64);
    expect_true("clall step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("no rts", c64.cpu.cpu.pc != (uint16_t)(TEST_RETURN_ADDRESS + 1u));
    expect_true("hostfs closed", lat_find(&c64, 2) < 0);
    expect_true("screen remains", lat_find(&c64, 3) >= 0);
    expect_true("one left", c64.bus.ram[ZP_LDTND] == 1);

    c64.bus.ram[ZP_LDTND] = 0;
    remove_tree(host_dir);
    printf("PASS: test_clall_foreign_at_head_with_hostfs\n");
}

int main(void)
{
    test_printer_basic_open_print_close();
    test_printer_disabled_does_not_claim();
    test_printer_non_device_does_not_claim();
    test_printer_chkin_chrin_not_claimed();
    test_printer_sealed_skips_host_write();
    test_clall_printer_only();
    test_clall_hostfs_only();
    test_clall_hostfs_then_printer();
    test_clall_printer_then_hostfs();
    test_clall_printer_hostfs_printer();
    test_clall_mixed_virtual_and_screen();
    test_clall_foreign_at_head_with_hostfs();
    printf("All printer KERNAL trap tests passed.\n");
    return 0;
}
