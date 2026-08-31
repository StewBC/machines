#include "c1541.h"
#include "c64.h"
#include "c64_hostfs.h"
#include "c64_rom.h"
#include "d64.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define HOSTFS_TEST_MKDIR(path) _mkdir(path)
#define HOSTFS_TEST_RMDIR(path) _rmdir(path)
#define HOSTFS_TEST_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
#define HOSTFS_TEST_MKDIR(path) mkdir((path), 0755)
#define HOSTFS_TEST_RMDIR(path) rmdir(path)
#define HOSTFS_TEST_ISDIR(mode) S_ISDIR(mode)
#endif

enum { HOSTFS_TEST_PATH_MAX = 512 };

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

static void load_nop_rom(c1541 *drive) {
    memset(drive->rom, 0xEA, C1541_ROM_SIZE);
    drive->rom_loaded = 1;
}

/* Recursively delete a scratch tree created under the system temp dir. */
static void remove_tree(const char *path)
{
    DIR *dir;
    struct dirent *de;
    char child[HOSTFS_TEST_PATH_MAX];

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
        if (HOSTFS_TEST_ISDIR(st.st_mode)) {
            remove_tree(child);
        } else {
            (void)remove(child);
        }
    }
    closedir(dir);
    if (HOSTFS_TEST_RMDIR(path) != 0) {
        fprintf(stderr, "warning: rmdir %s failed: %s\n", path, strerror(errno));
    }
}

/* Unique directory under TMPDIR (or /tmp). Caller must remove_tree(). */
static void make_tmpdir(char *out, size_t out_size)
{
    const char *base;
    char tmpl[HOSTFS_TEST_PATH_MAX];

    base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    if ((size_t)snprintf(
            tmpl, sizeof(tmpl), "%s/c64m-hostfs-XXXXXX", base) >= sizeof(tmpl)) {
        fail("tmpdir template too long");
    }
#if defined(_WIN32)
    {
        static int seq;
        for (;;) {
            if ((size_t)snprintf(
                    out, out_size, "%s/c64m-hostfs-%d", base, ++seq) >= out_size) {
                fail("tmpdir path too long");
            }
            if (HOSTFS_TEST_MKDIR(out) == 0) {
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

static uint8_t sectors_on_track(uint8_t track) {
    if (track <= 17) {
        return 21;
    }
    if (track <= 24) {
        return 19;
    }
    if (track <= 30) {
        return 18;
    }
    return 17;
}

static void set_bam_sector(uint8_t *bam, uint8_t track, uint8_t sector, int free_sector) {
    uint8_t *entry = &bam[4u + ((track - 1u) * 4u)];
    uint8_t mask = (uint8_t)(1u << (sector & 7u));
    uint8_t *byte = &entry[1u + (sector >> 3u)];
    int was_free = (*byte & mask) != 0;

    if (was_free == free_sector) {
        return;
    }
    if (free_sector) {
        *byte |= mask;
        entry[0]++;
    } else {
        *byte &= (uint8_t)~mask;
        entry[0]--;
    }
}

static void fill_d64_name(uint8_t *target, const char *name) {
    size_t i;
    memset(target, 0xa0, D64_DIRECTORY_NAME_SIZE);
    for (i = 0; i < D64_DIRECTORY_NAME_SIZE && name[i] != '\0'; ++i) {
        target[i] = (uint8_t)name[i];
    }
}

/* Minimal blank D64 with BAM title "TEST DISK" / id "ID" / DOS "2A". */
static uint8_t *make_blank_d64(void) {
    uint8_t *image;
    size_t offset;
    uint8_t *bam;
    uint8_t *directory;
    uint8_t track;

    image = (uint8_t *)calloc(1, D64_STANDARD_IMAGE_SIZE);
    if (image == NULL) {
        return NULL;
    }
    if (d64_track_sector_offset(18, 0, &offset) != D64_OK) {
        free(image);
        return NULL;
    }
    bam = &image[offset];
    bam[0] = 18;
    bam[1] = 1;
    bam[2] = 0x41;
    for (track = 1; track <= D64_TRACK_COUNT; ++track) {
        uint8_t sectors = sectors_on_track(track);
        uint8_t *entry = &bam[4u + ((track - 1u) * 4u)];
        uint8_t sector;

        entry[0] = sectors;
        entry[1] = entry[2] = entry[3] = 0;
        for (sector = 0; sector < sectors; ++sector) {
            entry[1u + (sector >> 3u)] |= (uint8_t)(1u << (sector & 7u));
        }
    }
    set_bam_sector(bam, 18, 0, 0);
    set_bam_sector(bam, 18, 1, 0);
    fill_d64_name(&bam[0x90], "TEST DISK");
    bam[0xa2] = 'I';
    bam[0xa3] = 'D';
    bam[0xa5] = '2';
    bam[0xa6] = 'A';
    if (d64_track_sector_offset(18, 1, &offset) != D64_OK) {
        free(image);
        return NULL;
    }
    directory = &image[offset];
    directory[0] = 0;
    directory[1] = 0xff;
    return image;
}

static void write_host_d64_with_prg(
    const char *dir,
    const char *basename,
    const char *prg_name,
    uint16_t load_addr,
    const uint8_t *body,
    size_t body_len) {
    char path[512];
    uint8_t *bytes;
    d64_image *image;
    d64_result result;
    uint8_t *prg;
    size_t prg_len;
    const uint8_t *out;
    size_t out_size;
    FILE *f;

    bytes = make_blank_d64();
    expect_true("blank d64", bytes != NULL);
    image = d64_image_create(bytes, D64_STANDARD_IMAGE_SIZE, &result);
    free(bytes);
    expect_true("parse blank", image != NULL && result == D64_OK);
    prg_len = 2u + body_len;
    prg = (uint8_t *)malloc(prg_len);
    expect_true("alloc prg", prg != NULL);
    prg[0] = (uint8_t)(load_addr & 0xffu);
    prg[1] = (uint8_t)(load_addr >> 8);
    if (body_len > 0) {
        memcpy(prg + 2, body, body_len);
    }
    expect_true(
        "write inside prg",
        d64_image_write_prg(
            image,
            (const uint8_t *)prg_name,
            strlen(prg_name),
            prg,
            prg_len,
            false) == D64_OK);
    free(prg);
    out = d64_image_bytes(image, &out_size);
    expect_true("d64 bytes", out != NULL && out_size == D64_STANDARD_IMAGE_SIZE);
    snprintf(path, sizeof(path), "%s/%s", dir, basename);
    f = fopen(path, "wb");
    expect_true("open host d64", f != NULL);
    expect_true("write host d64", fwrite(out, 1, out_size, f) == out_size);
    fclose(f);
    d64_image_destroy(image);
}

/* Assert ATN-ack DATA pull is absent after settling a VIA poke on drive 8. */
static void expect_no_atn_ack(c64_t *c64, c1541 *drive, const char *label) {
    c64->cia2.registers[0x00] = 0x08u;
    c64->cia2.registers[0x02] = 0x08u;
    drive->via1.ddrb = 0x10u;
    drive->via1.orb = 0x00u;
    c1541_advance_one_cycle(drive);
    c1541_advance_one_cycle(drive);
    c1541_advance_one_cycle(drive);
    if (c64->iec_external_pull & C64_IEC_DATA) {
        fprintf(stderr, "FAIL: %s: unexpected ATN-ack DATA pull\n", label);
        exit(1);
    }
}

static void test_mount_hostfs_basics(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    c64_drive_status st;

    make_tmpdir(dir, sizeof(dir));
    c64_init(&c64);
    expect_true(
        "mount hostfs",
        c64_mount_hostfs(&c64, 8, dir, true) == C64_DRIVE_STATUS_OK);
    expect_true("iec inactive", !c64_drive_iec_active(&c64, 8));
    expect_true("copy status", c64_copy_drive_status(&c64, 8, &st));
    expect_true("mounted", st.mounted);
    expect_true("powered", st.powered);
    expect_true("backend hostfs", st.backend == C64_DRIVE_BACKEND_HOSTFS);
    expect_true("kind hostfs", st.image_kind == C64_DRIVE_IMAGE_HOSTFS);
    expect_true("writable", st.writable);
    c64_unmount_drive(&c64, 8);
    expect_true("copy after eject", c64_copy_drive_status(&c64, 8, &st));
    expect_true("eject unmounted", !st.mounted);
    expect_true("eject backend none", st.backend == C64_DRIVE_BACKEND_NONE);
    expect_true("eject keeps power", st.powered);
    expect_true("eject still not iec", !c64_drive_iec_active(&c64, 8));
    remove_tree(dir);
    printf("PASS: test_mount_hostfs_basics\n");
}

static void test_hostfs_no_atn_ack(void) {
    static c64_t c64;
    static c1541 drive;
    char dir[HOSTFS_TEST_PATH_MAX];

    make_tmpdir(dir, sizeof(dir));
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    expect_true(
        "mount hostfs for atn",
        c64_mount_hostfs(&c64, 8, dir, true) == C64_DRIVE_STATUS_OK);
    /* ROM may be loaded on machine->drive8 as well; HostFS must still isolate. */
    load_nop_rom(&c64.drive8);
    expect_no_atn_ack(&c64, &drive, "hostfs mounted");
    remove_tree(dir);
    printf("PASS: test_hostfs_no_atn_ack\n");
}

static void test_powered_empty_no_atn_ack(void) {
    static c64_t c64;
    static c1541 drive;

    c64_init(&c64);
    c1541_init(&drive, &c64, 9);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    load_nop_rom(&c64.drive9);
    expect_true("power empty 9", c64_power_on_drive(&c64, 9));
    expect_true("powered empty", c64_drive_is_powered(&c64, 9));
    expect_true("not mounted", !c64.drives[1].mounted);
    expect_true("backend none", c64.drives[1].backend == C64_DRIVE_BACKEND_NONE);
    expect_true("not iec", !c64_drive_iec_active(&c64, 9));
    expect_no_atn_ack(&c64, &drive, "powered-empty");
    printf("PASS: test_powered_empty_no_atn_ack\n");
}

static void test_image_mount_still_atn_acks(void) {
    static c64_t c64;
    static c1541 drive;
    uint8_t *img = make_blank_d64();

    expect_true("alloc d64", img != NULL);
    c64_init(&c64);
    c1541_init(&drive, &c64, 8);
    load_nop_rom(&drive);
    c1541_reset(&drive);
    expect_true(
        "mount d64",
        c64_mount_d64(&c64, 8, img, C64_DRIVE_D64_STANDARD_SIZE, NULL, 0, "t", "", "", "", 0) ==
            C64_DRIVE_STATUS_OK);
    free(img);
    expect_true("iec active", c64_drive_iec_active(&c64, 8));

    c64.cia2.registers[0x00] = 0x08u;
    c64.cia2.registers[0x02] = 0x08u;
    drive.via1.ddrb = 0x10u;
    drive.via1.orb = 0x00u;
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    c1541_advance_one_cycle(&drive);
    expect_true("image atn ack", (c64.iec_external_pull & C64_IEC_DATA) != 0);
    printf("PASS: test_image_mount_still_atn_acks\n");
}

static void test_remount_image_over_hostfs(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    uint8_t *img = make_blank_d64();
    c64_drive_status st;

    expect_true("alloc d64", img != NULL);
    make_tmpdir(dir, sizeof(dir));
    c64_init(&c64);
    expect_true(
        "hostfs first",
        c64_mount_hostfs(&c64, 8, dir, true) == C64_DRIVE_STATUS_OK);
    expect_true(
        "d64 replaces hostfs",
        c64_mount_d64(&c64, 8, img, C64_DRIVE_D64_STANDARD_SIZE, NULL, 0, "t", "", "", "", 0) ==
            C64_DRIVE_STATUS_OK);
    free(img);
    expect_true("copy", c64_copy_drive_status(&c64, 8, &st));
    expect_true("backend image", st.backend == C64_DRIVE_BACKEND_IMAGE);
    expect_true("kind d64", st.image_kind == C64_DRIVE_IMAGE_D64);
    expect_true("iec after replace", c64_drive_iec_active(&c64, 8));
    expect_true("no hostfs handle", c64.drives[0].hostfs == NULL);
    remove_tree(dir);
    printf("PASS: test_remount_image_over_hostfs\n");
}

static void test_path_is_dir(void) {
    char dir[HOSTFS_TEST_PATH_MAX];
    make_tmpdir(dir, sizeof(dir));
    expect_true("dir is dir", c64_hostfs_path_is_dir(dir));
    expect_true("missing not dir", !c64_hostfs_path_is_dir("test_hostfs_no_such_path"));
    remove_tree(dir);
    printf("PASS: test_path_is_dir\n");
}

enum {
    TEST_RETURN_ADDRESS = 0x1233,
    TEST_FILENAME_BUFFER = 0x0200,
    TEST_RESET_VECTOR = 0xe000
};

static void reset_machine(c64_t *machine) {
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

static void write_host_prg(const char *dir, const char *basename, uint16_t load_addr, const uint8_t *body, size_t body_len) {
    char path[512];
    FILE *f;
    uint8_t hdr[2];

    snprintf(path, sizeof(path), "%s/%s", dir, basename);
    f = fopen(path, "wb");
    expect_true("open prg", f != NULL);
    hdr[0] = (uint8_t)(load_addr & 0xffu);
    hdr[1] = (uint8_t)(load_addr >> 8);
    expect_true("write hdr", fwrite(hdr, 1, 2, f) == 2);
    if (body_len > 0) {
        expect_true("write body", fwrite(body, 1, body_len, f) == body_len);
    }
    fclose(f);
}

static void setup_load_call(c64_t *machine, const char *name, uint8_t device, uint8_t secondary) {
    size_t length = strlen(name);
    size_t i;

    machine->cpu.cpu.pc = 0xffd5;
    machine->cpu.cpu.sp = 0x01fd;
    machine->bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    machine->bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
    machine->cpu.cpu.A = 0;
    machine->cpu.cpu.flags |= 0x01;
    machine->bus.ram[0xba] = device;
    machine->bus.ram[0xb9] = secondary;
    machine->bus.ram[0xb7] = (uint8_t)length;
    machine->bus.ram[0xbb] = (uint8_t)(TEST_FILENAME_BUFFER & 0xff);
    machine->bus.ram[0xbc] = (uint8_t)(TEST_FILENAME_BUFFER >> 8);
    for (i = 0; i < length; ++i) {
        machine->bus.ram[TEST_FILENAME_BUFFER + i] = (uint8_t)name[i];
    }
    machine->bus.ram[0x2b] = 0x01;
    machine->bus.ram[0x2c] = 0x08;
}

static void setup_save_call(
    c64_t *machine, const char *name, uint8_t device, uint16_t start, uint16_t end) {
    size_t length = strlen(name);
    size_t i;

    machine->cpu.cpu.pc = 0xffd8;
    machine->cpu.cpu.sp = 0x01fd;
    machine->bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    machine->bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
    machine->cpu.cpu.A = 0xc1;
    machine->cpu.cpu.X = (uint8_t)(end & 0xffu);
    machine->cpu.cpu.Y = (uint8_t)(end >> 8);
    machine->cpu.cpu.flags |= 0x01;
    machine->bus.ram[0xc1] = (uint8_t)(start & 0xffu);
    machine->bus.ram[0xc2] = (uint8_t)(start >> 8);
    machine->bus.ram[0xba] = device;
    machine->bus.ram[0xb7] = (uint8_t)length;
    machine->bus.ram[0xbb] = (uint8_t)(TEST_FILENAME_BUFFER & 0xff);
    machine->bus.ram[0xbc] = (uint8_t)(TEST_FILENAME_BUFFER >> 8);
    for (i = 0; i < length; ++i) {
        machine->bus.ram[TEST_FILENAME_BUFFER + i] = (uint8_t)name[i];
    }
}

static void expect_success_return(const c64_t *machine) {
    expect_true("carry clear", (machine->cpu.cpu.flags & 0x01u) == 0);
    expect_true("status clear", machine->bus.ram[0x90] == 0);
}

static void test_hostfs_load_save_traps(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    char path[512];
    char error[128];
    const uint8_t body[] = {0xA9, 0x01, 0x60}; /* LDA #$01 / RTS */
    struct stat st;
    FILE *f;
    uint8_t check[8];

    make_tmpdir(dir, sizeof(dir));
    write_host_prg(dir, "hello.prg", 0xC000u, body, sizeof(body));
    /* Extra: dir, unknown-ext file (listed as PRG), extensionless PRG, dotfile (hidden). */
    snprintf(path, sizeof(path), "%s/sub", dir);
    expect_true("mkdir sub", HOSTFS_TEST_MKDIR(path) == 0 || errno == EEXIST);
    snprintf(path, sizeof(path), "%s/notes.txt", dir);
    f = fopen(path, "wb");
    expect_true("notes", f != NULL);
    fputs("x", f);
    fclose(f);
    write_host_prg(dir, "fb64", 0xC100u, body, sizeof(body));
    snprintf(path, sizeof(path), "%s/.DS_Store", dir);
    f = fopen(path, "wb");
    expect_true("dotfile", f != NULL);
    fputs("x", f);
    fclose(f);

    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 9, dir, true) == C64_DRIVE_STATUS_OK);
    /* HELLO, NOTES.TXT, FB64, SUB — not .DS_Store */
    expect_true("catalog has entries", c64.drives[1].entry_count >= 4);
    {
        int found_fb64 = 0;
        int found_notes = 0;
        int found_dot = 0;
        size_t i;
        for (i = 0; i < c64.drives[1].entry_count; i++) {
            char name[17];
            size_t n = c64.drives[1].entries[i].filename_length;
            if (n > 16u) {
                n = 16u;
            }
            memcpy(name, c64.drives[1].entries[i].filename, n);
            name[n] = '\0';
            if (strcmp(name, "FB64") == 0) {
                found_fb64 = 1;
            }
            if (strcmp(name, "NOTES.TXT") == 0) {
                found_notes = 1;
            }
            if (name[0] == '.') {
                found_dot = 1;
            }
        }
        expect_true("lists extensionless FB64", found_fb64);
        expect_true("lists NOTES.TXT as PRG name", found_notes);
        expect_true("hides dotfiles", !found_dot);
    }

    /* LOAD "$",9 into BASIC (X/Y=$0801 as BASIC does) */
    setup_load_call(&c64, "$", 9, 0);
    expect_true("step $", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    /* FB-style LOAD "$" to a non-BASIC address must not move VARTAB. */
    {
        uint16_t vartab_before;
        uint16_t dest = 0x4000;
        setup_load_call(&c64, "$", 9, 0);
        c64.cpu.cpu.X = (uint8_t)(dest & 0xffu);
        c64.cpu.cpu.Y = (uint8_t)(dest >> 8);
        vartab_before = (uint16_t)c64.bus.ram[0x2d] | ((uint16_t)c64.bus.ram[0x2e] << 8);
        expect_true("step $ to $4000", c64_step_instruction(&c64, error, sizeof(error)));
        expect_success_return(&c64);
        expect_true(
            "vartab unchanged",
            ((uint16_t)c64.bus.ram[0x2d] | ((uint16_t)c64.bus.ram[0x2e] << 8)) ==
                vartab_before);
        /* Directory program link/next should be present at dest. */
        expect_true("dir at dest lo", c64_debug_read_ram(&c64, dest) != 0 ||
                                         c64_debug_read_ram(&c64, (uint16_t)(dest + 1)) != 0);
    }

    /* LOAD "HELLO",9,1 */
    setup_load_call(&c64, "HELLO", 9, 1);
    expect_true("step HELLO", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("loaded LDA", c64_debug_read_ram(&c64, 0xC000) == 0xA9);
    expect_true("loaded imm", c64_debug_read_ram(&c64, 0xC001) == 0x01);

    /* LOAD "FB64",9,1 — extensionless host file */
    setup_load_call(&c64, "FB64", 9, 1);
    expect_true("step FB64", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("fb64 LDA", c64_debug_read_ram(&c64, 0xC100) == 0xA9);

    /* LOAD "*",9,1 — first PRG in sorted catalog */
    setup_load_call(&c64, "*", 9, 1);
    expect_true("step *", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    /* SAVE "GAME",9 */
    c64.bus.ram[0x4000] = 0xEE;
    c64.bus.ram[0x4001] = 0xFF;
    setup_save_call(&c64, "GAME", 9, 0x4000, 0x4002);
    expect_true("step SAVE", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    snprintf(path, sizeof(path), "%s/GAME.prg", dir);
    expect_true("GAME.prg exists", stat(path, &st) == 0);
    f = fopen(path, "rb");
    expect_true("open GAME", f != NULL);
    expect_true("read GAME", fread(check, 1, 4, f) == 4);
    fclose(f);
    expect_true("GAME load lo", check[0] == 0x00);
    expect_true("GAME load hi", check[1] == 0x40);
    expect_true("GAME data0", check[2] == 0xEE);
    expect_true("GAME data1", check[3] == 0xFF);

    /* Second SAVE same name → file exists fail */
    setup_save_call(&c64, "GAME", 9, 0x4000, 0x4002);
    expect_true("step SAVE exists", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("carry set on exists", (c64.cpu.cpu.flags & 0x01u) != 0);
    expect_true(
        "status file exists",
        c64.drives[1].last_result == C64_DRIVE_STATUS_FILE_EXISTS);

    remove_tree(dir);
    printf("PASS: test_hostfs_load_save_traps\n");
}

static void test_hostfs_traps_with_emulate_1541(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    char error[128];
    const uint8_t body[] = {0x60};
    c64_config cfg;

    make_tmpdir(dir, sizeof(dir));
    write_host_prg(dir, "demo.prg", 0x8000u, body, sizeof(body));
    reset_machine(&c64);
    cfg = c64.config;
    cfg.emulate_1541 = 1;
    c64_set_config(&c64, &cfg);
    load_nop_rom(&c64.drive8);
    load_nop_rom(&c64.drive9);
    expect_true(
        "mount hostfs on 9",
        c64_mount_hostfs(&c64, 9, dir, true) == C64_DRIVE_STATUS_OK);

    setup_load_call(&c64, "DEMO", 9, 1);
    expect_true("step despite emulate_1541", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("demo byte", c64_debug_read_ram(&c64, 0x8000) == 0x60);
    remove_tree(dir);
    printf("PASS: test_hostfs_traps_with_emulate_1541\n");
}

static void test_hostfs_save_sealed(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    char path[512];
    char error[128];
    struct stat st;

    make_tmpdir(dir, sizeof(dir));
    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 8, dir, true) == C64_DRIVE_STATUS_OK);
    c64_set_replay_sealed(&c64, true);
    c64.bus.ram[0x3000] = 0x11;
    setup_save_call(&c64, "SEAL", 8, 0x3000, 0x3001);
    expect_true("sealed save step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    snprintf(path, sizeof(path), "%s/SEAL.prg", dir);
    expect_true("no host file", stat(path, &st) != 0);
    remove_tree(dir);
    printf("PASS: test_hostfs_save_sealed\n");
}

static void setup_open_call(
    c64_t *machine, const char *name, uint8_t la, uint8_t device, uint8_t sa)
{
    size_t length = strlen(name);
    size_t i;

    machine->cpu.cpu.pc = 0xffc0;
    machine->cpu.cpu.sp = 0x01fd;
    machine->bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    machine->bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
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
    machine->cpu.cpu.sp = 0x01fd;
    machine->bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    machine->bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
    machine->cpu.cpu.X = la;
    machine->cpu.cpu.flags |= 0x01;
}

static void test_hostfs_cd_channel(void)
{
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    char path[512];
    char error[128];
    const uint8_t body[] = {0xA9, 0x55, 0x60};
    const char *status;
    char parent_cmd[8];
    size_t i;

    make_tmpdir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/sub", dir);
    expect_true("mkdir sub", HOSTFS_TEST_MKDIR(path) == 0 || errno == EEXIST);
    write_host_prg(path, "nested.prg", 0xC000u, body, sizeof(body));
    write_host_prg(dir, "root.prg", 0x8000u, body, sizeof(body));

    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 9, dir, true) == C64_DRIVE_STATUS_OK);

    /* OPEN 1,9,15,"CD:SUB":CLOSE 1 */
    setup_open_call(&c64, "CD:SUB", 1, 9, 15);
    expect_true("open CD:SUB", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close after CD", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    /* LOAD "$",9 should list NESTED not ROOT */
    setup_load_call(&c64, "$", 9, 0);
    expect_true("load $ in sub", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("cwd catalog has nested", c64.drives[1].entry_count >= 1);
    {
        int found_nested = 0;
        int found_root = 0;
        for (i = 0; i < c64.drives[1].entry_count; i++) {
            char name[17];
            size_t n = c64.drives[1].entries[i].filename_length;
            if (n > 16u) {
                n = 16u;
            }
            memcpy(name, c64.drives[1].entries[i].filename, n);
            name[n] = '\0';
            if (strcmp(name, "NESTED") == 0) {
                found_nested = 1;
            }
            if (strcmp(name, "ROOT") == 0) {
                found_root = 1;
            }
        }
        expect_true("lists NESTED", found_nested);
        expect_true("no ROOT in sub", !found_root);
    }

    setup_load_call(&c64, "NESTED", 9, 1);
    expect_true("load nested", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("nested byte", c64_debug_read_ram(&c64, 0xC000) == 0xA9);

    /* Parent CD:_ (left arrow $5F) */
    parent_cmd[0] = 'C';
    parent_cmd[1] = 'D';
    parent_cmd[2] = ':';
    parent_cmd[3] = (char)0x5f;
    parent_cmd[4] = '\0';
    setup_open_call(&c64, parent_cmd, 1, 9, 15);
    expect_true("open CD:parent", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close parent", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    setup_load_call(&c64, "ROOT", 9, 1);
    expect_true("load root after parent", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    /* CD// back to root (already there) then missing dir */
    setup_open_call(&c64, "CD//", 1, 9, 15);
    expect_true("open CD//", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close root", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    setup_open_call(&c64, "CD:NOPE", 1, 9, 15);
    expect_true("open missing", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64); /* OPEN succeeds; DOS status carries the error */
    status = c64_hostfs_status(c64.drives[1].hostfs);
    expect_true("status set", status != NULL && status[0] == '6' && status[1] == '2');
    setup_close_call(&c64, 1);
    expect_true("close missing", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    /* Status read via OPEN/CHKIN/CHRIN */
    setup_open_call(&c64, "", 1, 9, 15);
    expect_true("open status", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    c64.cpu.cpu.pc = 0xffc6;
    c64.cpu.cpu.sp = 0x01fd;
    c64.bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    c64.bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
    c64.cpu.cpu.X = 1;
    c64.cpu.cpu.flags |= 0x01;
    expect_true("chkin", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    c64.cpu.cpu.pc = 0xffcf;
    c64.cpu.cpu.sp = 0x01fd;
    c64.bus.ram[0x01fe] = (uint8_t)(TEST_RETURN_ADDRESS & 0xff);
    c64.bus.ram[0x01ff] = (uint8_t)(TEST_RETURN_ADDRESS >> 8);
    expect_true("chrin", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("status first '0'", c64.cpu.cpu.A == '0');
    setup_close_call(&c64, 1);
    expect_true("close status", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);

    /* Non-CD command → syntax error status */
    setup_open_call(&c64, "S:FOO", 1, 9, 15);
    expect_true("open scratch unsupported", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    status = c64_hostfs_status(c64.drives[1].hostfs);
    expect_true("syntax status", status != NULL && status[0] == '3' && status[1] == '0');

    remove_tree(dir);
    printf("PASS: test_hostfs_cd_channel\n");
}

static void test_hostfs_cd_into_d64(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    char path[512];
    char error[128];
    const uint8_t inside_body[] = {0xA9, 0x42, 0x60};
    const uint8_t outside_body[] = {0xA9, 0x11, 0x60};
    char parent_cmd[8];
    FILE *f;
    struct stat st;
    size_t i;
    int found_game = 0;
    int found_outside = 0;
    int found_inside = 0;
    int game_is_dir = 0;

    make_tmpdir(dir, sizeof(dir));
    write_host_prg(dir, "outside.prg", 0x8000u, outside_body, sizeof(outside_body));
    write_host_d64_with_prg(
        dir, "game.d64", "INSIDE", 0xC000u, inside_body, sizeof(inside_body));
    /* .g64 must stay non-DIR / non-enterable. */
    snprintf(path, sizeof(path), "%s/skip.g64", dir);
    f = fopen(path, "wb");
    expect_true("dummy g64", f != NULL);
    fputs("not-a-real-g64", f);
    fclose(f);

    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 9, dir, true) == C64_DRIVE_STATUS_OK);
    expect_true("not in d64 yet", !c64_hostfs_in_d64(c64.drives[1].hostfs));

    setup_load_call(&c64, "$", 9, 0);
    expect_true("load $ host", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    for (i = 0; i < c64.drives[1].entry_count; i++) {
        char name[17];
        size_t n = c64.drives[1].entries[i].filename_length;
        if (n > 16u) {
            n = 16u;
        }
        memcpy(name, c64.drives[1].entries[i].filename, n);
        name[n] = '\0';
        if (strcmp(name, "GAME.D64") == 0) {
            found_game = 1;
            game_is_dir = (c64.drives[1].entries[i].type == C64_DRIVE_FILE_DIR);
        }
        if (strcmp(name, "OUTSIDE") == 0) {
            found_outside = 1;
        }
        if (strcmp(name, "INSIDE") == 0) {
            found_inside = 1;
        }
        if (strcmp(name, "SKIP.G64") == 0) {
            expect_true(
                "g64 not dir",
                c64.drives[1].entries[i].type != C64_DRIVE_FILE_DIR);
        }
    }
    expect_true("lists GAME.D64", found_game);
    expect_true("GAME.D64 is DIR", game_is_dir);
    expect_true("lists OUTSIDE", found_outside);
    expect_true("no INSIDE on host", !found_inside);

    setup_open_call(&c64, "CD:GAME.D64", 1, 9, 15);
    expect_true("open CD:GAME.D64", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close CD:GAME.D64", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("in d64", c64_hostfs_in_d64(c64.drives[1].hostfs));
    expect_true("still hostfs backend", c64.drives[1].backend == C64_DRIVE_BACKEND_HOSTFS);
    expect_true("not iec", !c64_drive_iec_active(&c64, 9));

    setup_load_call(&c64, "$", 9, 0);
    expect_true("load $ in d64", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    found_inside = 0;
    found_outside = 0;
    for (i = 0; i < c64.drives[1].entry_count; i++) {
        char name[17];
        size_t n = c64.drives[1].entries[i].filename_length;
        if (n > 16u) {
            n = 16u;
        }
        memcpy(name, c64.drives[1].entries[i].filename, n);
        name[n] = '\0';
        if (strcmp(name, "INSIDE") == 0) {
            found_inside = 1;
        }
        if (strcmp(name, "OUTSIDE") == 0) {
            found_outside = 1;
        }
    }
    expect_true("lists INSIDE", found_inside);
    expect_true("no OUTSIDE in d64", !found_outside);
    expect_true("bam title", strcmp(c64.drives[1].disk_title, "TEST DISK") == 0);
    expect_true("free not 65535", c64.drives[1].free_blocks != 65535u);

    setup_load_call(&c64, "INSIDE", 9, 1);
    expect_true("load inside", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("inside byte", c64_debug_read_ram(&c64, 0xC000) == 0xA9);
    expect_true("inside data", c64_debug_read_ram(&c64, 0xC001) == 0x42);

    c64.bus.ram[0x4000] = 0xEE;
    c64.bus.ram[0x4001] = 0xFF;
    setup_save_call(&c64, "NEWONE", 9, 0x4000, 0x4002);
    expect_true("save newone", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_save_call(&c64, "NEWONE", 9, 0x4000, 0x4002);
    expect_true("save exists step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("save exists carry", (c64.cpu.cpu.flags & 0x01u) != 0);

    snprintf(path, sizeof(path), "%s/game.d64", dir);
    expect_true("stat flushed", stat(path, &st) == 0);
    expect_true("flushed size", (size_t)st.st_size == D64_STANDARD_IMAGE_SIZE);

    c64_hostfs_set_writable(c64.drives[1].hostfs, false);
    c64.drives[1].writable = false;
    setup_save_call(&c64, "ROFAIL", 9, 0x4000, 0x4002);
    expect_true("save ro step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("save ro fail", (c64.cpu.cpu.flags & 0x01u) != 0);
    c64_hostfs_set_writable(c64.drives[1].hostfs, true);
    c64.drives[1].writable = true;

    parent_cmd[0] = 'C';
    parent_cmd[1] = 'D';
    parent_cmd[2] = ':';
    parent_cmd[3] = (char)0x5f;
    parent_cmd[4] = '\0';
    setup_open_call(&c64, parent_cmd, 1, 9, 15);
    expect_true("open CD:parent", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close parent", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("left d64", !c64_hostfs_in_d64(c64.drives[1].hostfs));

    setup_load_call(&c64, "OUTSIDE", 9, 1);
    expect_true("load outside", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("outside byte", c64_debug_read_ram(&c64, 0x8000) == 0xA9);

    setup_load_call(&c64, "INSIDE", 9, 1);
    expect_true("inside gone step", c64_step_instruction(&c64, error, sizeof(error)));
    expect_true("inside gone", (c64.cpu.cpu.flags & 0x01u) != 0);

    setup_open_call(&c64, "CD:GAME.D64", 1, 9, 15);
    expect_true("reenter", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close reenter", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("in d64 again", c64_hostfs_in_d64(c64.drives[1].hostfs));

    setup_open_call(&c64, "CD//", 1, 9, 15);
    expect_true("open CD//", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    setup_close_call(&c64, 1);
    expect_true("close root", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("root left d64", !c64_hostfs_in_d64(c64.drives[1].hostfs));

    setup_open_call(&c64, "CD:SKIP.G64", 1, 9, 15);
    expect_true("open g64", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    {
        const char *status = c64_hostfs_status(c64.drives[1].hostfs);
        expect_true(
            "g64 status 62",
            status != NULL && status[0] == '6' && status[1] == '2');
    }
    setup_close_call(&c64, 1);
    expect_true("close g64", c64_step_instruction(&c64, error, sizeof(error)));

    remove_tree(dir);
    printf("PASS: test_hostfs_cd_into_d64\n");
}

static void write_host_p00(
    const char *dir,
    const char *basename,
    const char *cbm_name,
    uint16_t load_addr,
    const uint8_t *body,
    size_t body_len) {
    char path[512];
    FILE *f;
    uint8_t hdr[26];
    uint8_t la[2];
    size_t i;
    size_t n;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "C64File", 7);
    n = strlen(cbm_name);
    if (n > 16u) {
        n = 16u;
    }
    for (i = 0; i < n; i++) {
        hdr[8u + i] = (uint8_t)cbm_name[i];
    }
    snprintf(path, sizeof(path), "%s/%s", dir, basename);
    f = fopen(path, "wb");
    expect_true("open p00", f != NULL);
    expect_true("write p00 hdr", fwrite(hdr, 1, sizeof(hdr), f) == sizeof(hdr));
    la[0] = (uint8_t)(load_addr & 0xffu);
    la[1] = (uint8_t)(load_addr >> 8);
    expect_true("write p00 la", fwrite(la, 1, 2, f) == 2);
    if (body_len > 0) {
        expect_true("write p00 body", fwrite(body, 1, body_len, f) == body_len);
    }
    fclose(f);
}

static void test_hostfs_p00_load(void) {
    static c64_t c64;
    char dir[HOSTFS_TEST_PATH_MAX];
    char error[128];
    const uint8_t body[] = {0xA9, 0x77, 0x60};
    size_t i;
    int found = 0;

    make_tmpdir(dir, sizeof(dir));
    write_host_p00(dir, "CARCRASH.P00", "CAR CRASH", 0xC000u, body, sizeof(body));
    write_host_prg(dir, "plain.prg", 0x8000u, body, sizeof(body));

    reset_machine(&c64);
    expect_true(
        "mount",
        c64_mount_hostfs(&c64, 9, dir, true) == C64_DRIVE_STATUS_OK);

    setup_load_call(&c64, "$", 9, 0);
    expect_true("load $", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    for (i = 0; i < c64.drives[1].entry_count; i++) {
        char name[17];
        size_t n = c64.drives[1].entries[i].filename_length;
        if (n > 16u) {
            n = 16u;
        }
        memcpy(name, c64.drives[1].entries[i].filename, n);
        name[n] = '\0';
        if (strcmp(name, "CAR CRASH") == 0) {
            found = 1;
            expect_true(
                "p00 is PRG",
                c64.drives[1].entries[i].type == C64_DRIVE_FILE_PRG);
        }
        expect_true("not listed as CARCRASH.P00", strcmp(name, "CARCRASH.P00") != 0);
    }
    expect_true("lists header name", found);

    setup_load_call(&c64, "CAR CRASH", 9, 1);
    expect_true("load p00", c64_step_instruction(&c64, error, sizeof(error)));
    expect_success_return(&c64);
    expect_true("payload at C000", c64_debug_read_ram(&c64, 0xC000) == 0xA9);
    expect_true("payload data", c64_debug_read_ram(&c64, 0xC001) == 0x77);
    /* Must not have loaded 'C''6' from the PC64 magic as the load address. */
    expect_true("not loaded at 3643", c64_debug_read_ram(&c64, 0x3643) != 0xA9);

    remove_tree(dir);
    printf("PASS: test_hostfs_p00_load\n");
}

int main(void) {
    test_path_is_dir();
    test_mount_hostfs_basics();
    test_hostfs_no_atn_ack();
    test_powered_empty_no_atn_ack();
    test_image_mount_still_atn_acks();
    test_remount_image_over_hostfs();
    test_hostfs_load_save_traps();
    test_hostfs_traps_with_emulate_1541();
    test_hostfs_save_sealed();
    test_hostfs_cd_channel();
    test_hostfs_cd_into_d64();
    test_hostfs_p00_load();
    printf("All HostFS mount/trap tests passed.\n");
    return 0;
}
