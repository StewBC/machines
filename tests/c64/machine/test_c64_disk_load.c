#include "c64.h"
#include "c64_rom.h"
#include "d64.h"

#include "../test_asset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef C64M_SOURCE_DIR
#define C64M_SOURCE_DIR "."
#endif

enum {
    TEST_RESET_VECTOR = 0xe000,
    TEST_RETURN_ADDRESS = 0x1233,
    TEST_FILENAME_BUFFER = 0x0200
};

static void fail(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

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

static void expect_u16(const char *name, uint16_t expected, uint16_t actual) {
    if (expected != actual) {
        fprintf(stderr, "%s: expected %04x, got %04x\n", name, expected, actual);
        exit(1);
    }
}

static int read_file(const char *path, uint8_t **out_bytes, size_t *out_size) {
    FILE *file;
    long size;
    uint8_t *bytes;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 1;
    }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return 1;
    }
    bytes = (uint8_t *)malloc((size_t)size);
    if (bytes == NULL) {
        fclose(file);
        return 1;
    }
    if (fread(bytes, 1, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return 1;
    }
    fclose(file);
    *out_bytes = bytes;
    *out_size = (size_t)size;
    return 0;
}

static void build_roms(c64_rom_set *roms) {
    c64_rom_set_init(roms);
    roms->has_basic = true;
    roms->has_kernal = true;
    roms->has_character = true;
    memset(roms->basic, 0xea, sizeof(roms->basic));
    memset(roms->kernal, 0xea, sizeof(roms->kernal));
    memset(roms->character, 0, sizeof(roms->character));
    roms->kernal[0x1ffc] = (uint8_t)(TEST_RESET_VECTOR & 0xff);
    roms->kernal[0x1ffd] = (uint8_t)(TEST_RESET_VECTOR >> 8);
}

static void reset_machine(c64_t *machine) {
    c64_rom_set roms;
    char error[256];

    build_roms(&roms);
    c64_init(machine);
    expect_true("install ROMs", c64_install_roms(machine, &roms, error, sizeof(error)));
    expect_true("reset machine", c64_reset(machine, error, sizeof(error)));
}

static void mount_fixture_device(c64_t *machine, uint8_t device, const char *filename, d64_image **out_image, uint8_t **out_bytes) {
    char path[512];
    uint8_t *bytes = NULL;
    size_t size = 0;
    d64_result result;
    d64_image *image;
    const d64_disk_info *info;
    d64_directory_entry title_entry;
    size_t count;
    size_t i;
    c64_drive_directory_entry *entries;
    d64_directory_entry d64_entry;
    char title[17];
    char disk_id[3];
    char dos_type[3];

    snprintf(path, sizeof(path), "%s/assets/c64/disks/%s", C64M_SOURCE_DIR, filename);
    if (read_file(path, &bytes, &size) != 0) {
        fail("failed to read disk fixture");
    }
    image = d64_image_create(bytes, size, &result);
    if (image == NULL) {
        fprintf(stderr, "failed to parse disk fixture: %s\n", d64_result_string(result));
        exit(1);
    }

    count = d64_image_directory_count(image);
    entries = (c64_drive_directory_entry *)calloc(count, sizeof(*entries));
    if (entries == NULL) {
        fail("failed to allocate entries");
    }
    for (i = 0; i < count; ++i) {
        expect_true("copy directory entry", d64_image_directory_entry(image, i, &d64_entry) == D64_OK);
        entries[i].raw_type = d64_entry.raw_type;
        entries[i].type = (c64_drive_file_type)d64_entry.type;
        entries[i].first_track = d64_entry.first_track;
        entries[i].first_sector = d64_entry.first_sector;
        memcpy(entries[i].filename, d64_entry.filename, sizeof(entries[i].filename));
        entries[i].filename_length = d64_entry.filename_length;
        entries[i].block_count = d64_entry.block_count;
    }

    title[0] = '\0';
    disk_id[0] = '\0';
    dos_type[0] = '\0';
    info = d64_image_disk_info(image);
    if (info != NULL) {
        memset(&title_entry, 0, sizeof(title_entry));
        memcpy(title_entry.filename, info->title, D64_DIRECTORY_NAME_SIZE);
        title_entry.filename_length = info->title_length;
        (void)d64_entry_name_ascii(&title_entry, title, sizeof(title));
        disk_id[0] = (char)info->disk_id[0];
        disk_id[1] = (char)info->disk_id[1];
        disk_id[2] = '\0';
        dos_type[0] = (char)info->dos_type[0];
        dos_type[1] = (char)info->dos_type[1];
        dos_type[2] = '\0';
    }

    expect_true("mount odell", c64_mount_d64(
        machine,
        device,
        bytes,
        D64_STANDARD_IMAGE_SIZE,
        entries,
        count,
        filename,
        title,
        disk_id,
        dos_type,
        info != NULL ? info->free_blocks : 0) == C64_DRIVE_STATUS_OK);
    free(entries);
    *out_image = image;
    *out_bytes = bytes;
}

static void mount_fixture(c64_t *machine, const char *filename, d64_image **out_image, uint8_t **out_bytes) {
    mount_fixture_device(machine, 8, filename, out_image, out_bytes);
}

static void mount_odell(c64_t *machine, d64_image **out_image, uint8_t **out_bytes) {
    mount_fixture(machine, "ODELLLAK.D64", out_image, out_bytes);
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
    c64_t *machine,
    const char *name,
    uint8_t device,
    uint16_t start,
    uint16_t end) {
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
    machine->bus.ram[0xb9] = 0;
    machine->bus.ram[0xb7] = (uint8_t)length;
    machine->bus.ram[0xbb] = (uint8_t)(TEST_FILENAME_BUFFER & 0xff);
    machine->bus.ram[0xbc] = (uint8_t)(TEST_FILENAME_BUFFER >> 8);
    for (i = 0; i < length; ++i) {
        machine->bus.ram[TEST_FILENAME_BUFFER + i] = (uint8_t)name[i];
    }
}

static void expect_success_return(const c64_t *machine) {
    expect_u16("trap return PC", (uint16_t)(TEST_RETURN_ADDRESS + 1), machine->cpu.cpu.pc);
    expect_true("carry clear", (machine->cpu.cpu.flags & 0x01u) == 0);
    expect_u8("KERNAL status clear", 0, machine->bus.ram[0x90]);
}

static void expect_failure_return(const c64_t *machine) {
    expect_u16("trap return PC", (uint16_t)(TEST_RETURN_ADDRESS + 1), machine->cpu.cpu.pc);
    expect_true("carry set", (machine->cpu.cpu.flags & 0x01u) != 0);
}

static uint16_t read_le16_ram(const c64_t *machine, uint16_t address) {
    return (uint16_t)c64_debug_read_ram(machine, address) |
        ((uint16_t)c64_debug_read_ram(machine, (uint16_t)(address + 1u)) << 8);
}

static void decode_basic_program(const c64_t *machine, char *out, size_t out_size) {
    uint16_t address = 0x0801;
    size_t out_index = 0;
    unsigned guard = 0;

    if (out_size == 0) {
        return;
    }

    while (guard++ < 256) {
        uint16_t next = read_le16_ram(machine, address);
        uint16_t line;
        uint16_t cursor;

        if (next == 0) {
            break;
        }
        if (next <= address + 4u) {
            fail("invalid BASIC line link");
        }
        line = read_le16_ram(machine, (uint16_t)(address + 2u));
        out_index += (size_t)snprintf(&out[out_index], out_index < out_size ? out_size - out_index : 0, "%u ", line);
        cursor = (uint16_t)(address + 4u);
        while (cursor < next - 1u) {
            uint8_t ch = c64_debug_read_ram(machine, cursor++);
            if (out_index + 1u < out_size) {
                out[out_index++] = (char)ch;
            }
        }
        if (out_index + 1u < out_size) {
            out[out_index++] = '\n';
        }
        address = next;
    }

    out[out_index < out_size ? out_index : out_size - 1u] = '\0';
}

static void expect_basic_contains(const c64_t *machine, const char *needle) {
    char text[4096];

    decode_basic_program(machine, text, sizeof(text));
    if (strstr(text, needle) == NULL) {
        fprintf(stderr, "BASIC directory did not contain '%s'\nDecoded:\n%s\n", needle, text);
        exit(1);
    }
}

static void expect_basic_end_pointer(const c64_t *machine) {
    uint16_t address = 0x0801;
    uint16_t next = 0;
    unsigned guard = 0;

    while (guard++ < 256) {
        next = read_le16_ram(machine, address);
        if (next == 0) {
            break;
        }
        address = next;
    }
    expect_u16("VARTAB matches BASIC end", (uint16_t)(address + 2u), read_le16_ram(machine, 0x2d));
    expect_u16("ARYTAB matches BASIC end", (uint16_t)(address + 2u), read_le16_ram(machine, 0x2f));
    expect_u16("STREND matches BASIC end", (uint16_t)(address + 2u), read_le16_ram(machine, 0x31));
}

static void test_load_prg_secondary_one(void) {
    c64_t machine;
    d64_image *image;
    uint8_t *bytes;
    d64_directory_entry entry;
    d64_file_data file = {0};
    char error[256];
    uint16_t load_address;
    uint16_t end_address;
    size_t i;

    reset_machine(&machine);
    mount_odell(&machine, &image, &bytes);
    expect_true("find MENU1", d64_image_find_entry_ascii(image, "MENU1", &entry) == D64_OK);
    expect_true("extract MENU1", d64_image_extract_prg(image, &entry, &file) == D64_OK);
    load_address = (uint16_t)file.bytes[0] | ((uint16_t)file.bytes[1] << 8);
    end_address = (uint16_t)(load_address + file.size - 2u);

    setup_load_call(&machine, "MENU1", 8, 1);
    expect_true("step load trap", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u16("end address XY", end_address, (uint16_t)machine.cpu.cpu.X | ((uint16_t)machine.cpu.cpu.Y << 8));
    for (i = 2; i < file.size && i < 18; ++i) {
        expect_u8("loaded byte", file.bytes[i], c64_debug_read_ram(&machine, (uint16_t)(load_address + i - 2u)));
    }

    d64_file_data_free(&file);
    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_load_prg_basic_semantics(void) {
    c64_t machine;
    d64_image *image;
    uint8_t *bytes;
    d64_directory_entry entry;
    d64_file_data file = {0};
    char error[256];
    uint16_t basic_start = 0x0801;
    uint16_t end_address;

    reset_machine(&machine);
    mount_odell(&machine, &image, &bytes);
    expect_true("find MENU1", d64_image_find_entry_ascii(image, "MENU1", &entry) == D64_OK);
    expect_true("extract MENU1", d64_image_extract_prg(image, &entry, &file) == D64_OK);
    end_address = (uint16_t)(basic_start + file.size - 2u);

    setup_load_call(&machine, "MENU1", 8, 0);
    expect_true("step basic load trap", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u8("basic load first byte", file.bytes[2], c64_debug_read_ram(&machine, basic_start));
    expect_u16("VARTAB updated", end_address, (uint16_t)machine.bus.ram[0x2d] | ((uint16_t)machine.bus.ram[0x2e] << 8));
    expect_u16("ARYTAB updated", end_address, (uint16_t)machine.bus.ram[0x2f] | ((uint16_t)machine.bus.ram[0x30] << 8));
    expect_u16("STREND updated", end_address, (uint16_t)machine.bus.ram[0x31] | ((uint16_t)machine.bus.ram[0x32] << 8));

    d64_file_data_free(&file);
    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_failures_and_device_fallthrough(void) {
    c64_t machine;
    d64_image *image;
    uint8_t *bytes;
    char error[256];

    reset_machine(&machine);
    setup_load_call(&machine, "MENU1", 8, 1);
    machine.bus.ram[0x3000] = 0x5a;
    expect_true("no disk trap", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);
    expect_u8("sentinel unchanged after no disk", 0x5a, machine.bus.ram[0x3000]);

    reset_machine(&machine);
    mount_odell(&machine, &image, &bytes);
    machine.bus.ram[0x3000] = 0xa5;
    setup_load_call(&machine, "NO SUCH FILE", 8, 1);
    expect_true("missing file trap", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);
    expect_u8("sentinel unchanged after missing", 0xa5, machine.bus.ram[0x3000]);

    setup_load_call(&machine, "LAKESTR.TXT", 8, 1);
    expect_true("SEQ file trap", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);

    setup_load_call(&machine, "MENU1", 10, 1);
    expect_true("device 10 falls through", c64_step_instruction(&machine, error, sizeof(error)));
    expect_u16("device 10 executed ROM NOP", 0xffd6, machine.cpu.cpu.pc);

    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_directory_load_blank_and_odell(void) {
    c64_t machine;
    d64_image *image;
    uint8_t *bytes;
    char error[256];

    reset_machine(&machine);
    mount_fixture(&machine, "blank.d64", &image, &bytes);
    setup_load_call(&machine, "$", 8, 0);
    expect_true("blank directory load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_basic_contains(&machine, "BLOCKS FREE.");
    expect_basic_end_pointer(&machine);
    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);

    reset_machine(&machine);
    mount_odell(&machine, &image, &bytes);
    setup_load_call(&machine, "$", 8, 0);
    expect_true("odell directory load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_basic_contains(&machine, "ASS PRESENTS:");
    expect_basic_contains(&machine, "MENU1");
    expect_basic_contains(&machine, "LAKESPT.BIN");
    expect_basic_contains(&machine, "LAKESTR.TXT");
    expect_basic_contains(&machine, "SEQ");
    expect_basic_end_pointer(&machine);
    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_wildcard_and_case_matching(void) {
    c64_t machine;
    d64_image *image;
    uint8_t *bytes;
    d64_directory_entry entry;
    d64_file_data file = {0};
    char error[256];
    uint16_t load_address;

    reset_machine(&machine);
    mount_odell(&machine, &image, &bytes);

    setup_load_call(&machine, "menu1", 8, 1);
    expect_true("lowercase exact load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);

    expect_true("find first PRG", d64_image_find_entry_ascii(image, "ODELL LAKE", &entry) == D64_OK);
    expect_true("extract first PRG", d64_image_extract_prg(image, &entry, &file) == D64_OK);
    load_address = (uint16_t)file.bytes[0] | ((uint16_t)file.bytes[1] << 8);
    setup_load_call(&machine, "*", 8, 1);
    expect_true("star wildcard load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u8("star loaded first PRG", file.bytes[2], c64_debug_read_ram(&machine, load_address));
    d64_file_data_free(&file);

    expect_true("find LAKESPT.BIN", d64_image_find_entry_ascii(image, "LAKESPT.BIN", &entry) == D64_OK);
    expect_true("extract LAKESPT.BIN", d64_image_extract_prg(image, &entry, &file) == D64_OK);
    load_address = (uint16_t)file.bytes[0] | ((uint16_t)file.bytes[1] << 8);
    setup_load_call(&machine, "LAKE*", 8, 1);
    expect_true("prefix wildcard load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u8("prefix loaded deterministic PRG", file.bytes[2], c64_debug_read_ram(&machine, load_address));
    d64_file_data_free(&file);

    setup_load_call(&machine, "MENU?", 8, 1);
    expect_true("question wildcard load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);

    setup_load_call(&machine, "GEN*", 8, 1);
    expect_true("wildcard skips SEQ and misses", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);

    setup_load_call(&machine, "NO*", 8, 1);
    expect_true("missing wildcard load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);

    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void put_fake_directory_name(c64_drive_directory_entry *entry, const char *name) {
    size_t i;

    memset(entry->filename, 0xa0, sizeof(entry->filename));
    entry->filename_length = strlen(name);
    for (i = 0; i < entry->filename_length && i < sizeof(entry->filename); ++i) {
        entry->filename[i] = (uint8_t)name[i];
    }
}

static void test_malformed_mounted_chains_fail_safely(void) {
    c64_t machine;
    uint8_t *image;
    c64_drive_directory_entry entry;
    char error[256];
    size_t offset;

    reset_machine(&machine);
    image = (uint8_t *)calloc(1, C64_DRIVE_D64_STANDARD_SIZE);
    if (image == NULL) {
        fail("failed to allocate malformed image");
    }
    memset(&entry, 0, sizeof(entry));
    entry.type = C64_DRIVE_FILE_PRG;
    entry.raw_type = 0x82;
    entry.first_track = 1;
    entry.first_sector = 0;
    put_fake_directory_name(&entry, "LOOP");
    expect_true("mount loop image", c64_mount_d64(&machine, 8, image, C64_DRIVE_D64_STANDARD_SIZE, &entry, 1, "loop.d64", "LOOP", "  ", "  ", 0) == C64_DRIVE_STATUS_OK);
    expect_true("sector offset", d64_track_sector_offset(1, 0, &offset) == D64_OK);
    machine.drives[0].image_bytes[offset] = 1;
    machine.drives[0].image_bytes[offset + 1] = 0;
    setup_load_call(&machine, "LOOP", 8, 1);
    machine.bus.ram[0x3000] = 0x77;
    expect_true("loop chain load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);
    expect_u8("loop sentinel unchanged", 0x77, machine.bus.ram[0x3000]);
    c64_unmount_all_drives(&machine);

    memset(&entry, 0, sizeof(entry));
    entry.type = C64_DRIVE_FILE_PRG;
    entry.raw_type = 0x82;
    entry.first_track = 35;
    entry.first_sector = 17;
    put_fake_directory_name(&entry, "BAD");
    expect_true("mount bad sector image", c64_mount_d64(&machine, 8, image, C64_DRIVE_D64_STANDARD_SIZE, &entry, 1, "bad.d64", "BAD", "  ", "  ", 0) == C64_DRIVE_STATUS_OK);
    setup_load_call(&machine, "BAD", 8, 1);
    machine.bus.ram[0x3000] = 0x88;
    expect_true("bad sector load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);
    expect_u8("bad sector sentinel unchanged", 0x88, machine.bus.ram[0x3000]);
    c64_unmount_all_drives(&machine);
    free(image);
}

static void test_device_9_loads_and_independent_slots(void) {
    c64_t machine;
    d64_image *blank_image;
    d64_image *odell_image;
    uint8_t *blank_bytes;
    uint8_t *odell_bytes;
    d64_directory_entry entry;
    d64_file_data file = {0};
    char error[256];
    uint16_t load_address;
    c64_drive_status status8;
    c64_drive_status status9;

    reset_machine(&machine);
    mount_fixture_device(&machine, 8, "blank.d64", &blank_image, &blank_bytes);
    mount_fixture_device(&machine, 9, "ODELLLAK.D64", &odell_image, &odell_bytes);

    expect_true("copy status 8", c64_copy_drive_status(&machine, 8, &status8));
    expect_true("copy status 9", c64_copy_drive_status(&machine, 9, &status9));
    expect_true("device 8 mounted", status8.mounted);
    expect_true("device 9 mounted", status9.mounted);
    expect_true("device 8 and 9 names differ", strcmp(status8.display_name, status9.display_name) != 0);

    setup_load_call(&machine, "$", 9, 0);
    expect_true("device 9 directory load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_basic_contains(&machine, "MENU1");

    expect_true("find MENU1", d64_image_find_entry_ascii(odell_image, "MENU1", &entry) == D64_OK);
    expect_true("extract MENU1", d64_image_extract_prg(odell_image, &entry, &file) == D64_OK);
    load_address = (uint16_t)file.bytes[0] | ((uint16_t)file.bytes[1] << 8);
    setup_load_call(&machine, "MENU1", 9, 1);
    expect_true("device 9 PRG load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u8("device 9 PRG byte", file.bytes[2], c64_debug_read_ram(&machine, load_address));

    c64_unmount_drive(&machine, 9);
    expect_true("copy status 8 after unmount 9", c64_copy_drive_status(&machine, 8, &status8));
    expect_true("copy status 9 after unmount 9", c64_copy_drive_status(&machine, 9, &status9));
    expect_true("device 8 remains mounted", status8.mounted);
    expect_true("device 9 unmounted", !status9.mounted);

    setup_load_call(&machine, "MENU1", 9, 1);
    expect_true("device 9 missing disk fails", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);

    d64_file_data_free(&file);
    d64_image_destroy(blank_image);
    d64_image_destroy(odell_image);
    free(blank_bytes);
    free(odell_bytes);
    c64_unmount_all_drives(&machine);
}

static void test_kernal_save_trap_writes_prg(void) {
    c64_t machine;
    d64_image *mounted_image = NULL;
    uint8_t *mounted_bytes = NULL;
    d64_image *saved_image;
    d64_directory_entry entry;
    d64_file_data file;
    d64_result result;
    char error[256];
    uint8_t expected[] = {0x01, 0x08, 0x11, 0x22, 0x33, 0x44};
    size_t i;

    reset_machine(&machine);
    mount_fixture(&machine, "blank.d64", &mounted_image, &mounted_bytes);
    expect_true("enable writable disk", c64_set_drive_writable(&machine, 8, true));
    for (i = 2; i < sizeof(expected); ++i) {
        machine.bus.ram[0x0801u + (uint16_t)(i - 2u)] = expected[i];
    }

    setup_save_call(&machine, "SAVED", 8, 0x0801, 0x0805);
    expect_true("SAVE trap step", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_true("disk marked dirty", machine.drives[0].dirty);
    expect_true("disk still writable", machine.drives[0].writable);

    saved_image = d64_image_create(
        machine.drives[0].image_bytes,
        machine.drives[0].image_size,
        &result);
    expect_true("saved image parses", saved_image != NULL && result == D64_OK);
    expect_true("find saved file", d64_image_find_entry_ascii(saved_image, "SAVED", &entry) == D64_OK);
    memset(&file, 0, sizeof(file));
    expect_true("extract saved file", d64_image_extract_prg(saved_image, &entry, &file) == D64_OK);
    expect_u16("saved file size", (uint16_t)sizeof(expected), (uint16_t)file.size);
    if (file.size != sizeof(expected) || memcmp(file.bytes, expected, sizeof(expected)) != 0) {
        fail("saved PRG bytes mismatch");
    }

    d64_file_data_free(&file);
    d64_image_destroy(saved_image);
    d64_image_destroy(mounted_image);
    free(mounted_bytes);
    c64_unmount_all_drives(&machine);
}

static uint8_t *make_blank_d64_of_size(size_t size) {
    uint8_t *image;
    size_t offset;
    uint8_t *bam;
    uint8_t track;

    image = (uint8_t *)calloc(1, size);
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
    for (track = 1; track <= D64_DOS_TRACK_COUNT; ++track) {
        uint8_t sectors = d64_sectors_per_track(track);
        uint8_t *entry = &bam[4u + ((track - 1u) * 4u)];
        uint8_t sector;

        entry[0] = sectors;
        for (sector = 0; sector < sectors; ++sector) {
            entry[1u + (sector >> 3u)] |= (uint8_t)(1u << (sector & 7u));
        }
    }
    {
        uint8_t *e18 = &bam[4u + (17u * 4u)];
        e18[1] &= (uint8_t)~0x03u;
        e18[0] = (uint8_t)(d64_sectors_per_track(18) - 2u);
    }
    bam[0xa2] = 'I';
    bam[0xa3] = 'D';
    bam[0xa5] = '2';
    bam[0xa6] = 'A';
    if (d64_track_sector_offset(18, 1, &offset) != D64_OK) {
        free(image);
        return NULL;
    }
    image[offset] = 0;
    image[offset + 1] = 0xff;
    return image;
}

static void test_canonical_d64_sizes_mount_full_blob(void) {
    static const size_t sizes[] = {
        D64_STANDARD_IMAGE_SIZE,
        D64_ERROR_INFO_IMAGE_SIZE,
        D64_40TRACK_IMAGE_SIZE,
        D64_40TRACK_ERROR_SIZE,
        D64_42TRACK_IMAGE_SIZE,
        D64_42TRACK_ERROR_SIZE
    };
    c64_t machine;
    size_t i;

    reset_machine(&machine);
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        uint8_t *bytes;
        c64_drive_status_result status;

        bytes = (uint8_t *)calloc(1, sizes[i]);
        if (bytes == NULL) {
            fail("failed to allocate canonical D64");
        }
        bytes[sizes[i] - 1u] = 0xa5;
        status = c64_mount_d64_ex(
            &machine, 8, bytes, sizes[i], NULL, 0, "size.d64", "", "", "", 0, false);
        expect_true("canonical size mounts", status == C64_DRIVE_STATUS_OK);
        expect_true("mounted at full size", machine.drives[0].mounted);
        expect_true("image_size is full blob", machine.drives[0].image_size == sizes[i]);
        expect_u8("last blob byte kept", 0xa5, machine.drives[0].image_bytes[sizes[i] - 1u]);
        c64_unmount_all_drives(&machine);
        free(bytes);
    }

    {
        uint8_t dummy[4] = {0};
        c64_drive_status_result status = c64_mount_d64_ex(
            &machine, 8, dummy, 174849u, NULL, 0, "odd.d64", "", "", "", 0, false);
        expect_true(
            "non-canonical size rejected",
            status == C64_DRIVE_STATUS_UNSUPPORTED_IMAGE);
        expect_true("odd size not mounted", !machine.drives[0].mounted);
    }
}

static void test_40track_mount_does_not_truncate(void) {
    c64_t machine;
    uint8_t *bytes;
    d64_image *image;
    d64_result result;
    c64_drive_status_result status;

    reset_machine(&machine);
    bytes = (uint8_t *)calloc(1, D64_40TRACK_IMAGE_SIZE);
    if (bytes == NULL) {
        fail("failed to allocate 40-track image");
    }
    bytes[D64_STANDARD_IMAGE_SIZE] = 0x5a;
    image = d64_image_create(bytes, D64_40TRACK_IMAGE_SIZE, &result);
    expect_true("40-track parse succeeds", image != NULL && result == D64_OK);

    status = c64_mount_d64_ex(
        &machine,
        8,
        bytes,
        D64_40TRACK_IMAGE_SIZE,
        NULL,
        0,
        "matrix.d64",
        "",
        "",
        "",
        0,
        false);
    expect_true("40-track mount succeeds", status == C64_DRIVE_STATUS_OK);
    expect_true("40-track slot mounted", machine.drives[0].mounted);
    expect_true(
        "196608 mounts at full size",
        machine.drives[0].image_size == D64_40TRACK_IMAGE_SIZE);
    expect_true(
        "196608 never mounts as 174848",
        machine.drives[0].image_size != D64_STANDARD_IMAGE_SIZE);
    expect_u8(
        "extra-track byte kept",
        0x5a,
        machine.drives[0].image_bytes[D64_STANDARD_IMAGE_SIZE]);

    d64_image_destroy(image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_40track_trap_load_extra_track_chain(void) {
    c64_t machine;
    uint8_t *bytes;
    c64_drive_directory_entry entries[2];
    size_t offset;
    char error[256];

    reset_machine(&machine);
    bytes = (uint8_t *)calloc(1, D64_40TRACK_IMAGE_SIZE);
    if (bytes == NULL) {
        fail("failed to allocate 40-track trap image");
    }

    memset(entries, 0, sizeof(entries));
    entries[0].type = C64_DRIVE_FILE_PRG;
    entries[0].raw_type = 0x82;
    entries[0].first_track = 18;
    entries[0].first_sector = 2;
    entries[0].block_count = 2;
    put_fake_directory_name(&entries[0], "EXTPRG");
    entries[1].type = C64_DRIVE_FILE_PRG;
    entries[1].raw_type = 0x82;
    entries[1].first_track = 36;
    entries[1].first_sector = 1;
    entries[1].block_count = 1;
    put_fake_directory_name(&entries[1], "T36PRG");

    expect_true("18/2 offset", d64_track_sector_offset(18, 2, &offset) == D64_OK);
    bytes[offset] = 36;
    bytes[offset + 1] = 0;
    bytes[offset + 2] = 0x01;
    bytes[offset + 3] = 0x08;
    bytes[offset + 4] = 0xaa;

    expect_true("36/0 offset", d64_track_sector_offset(36, 0, &offset) == D64_OK);
    bytes[offset] = 0;
    bytes[offset + 1] = 3;
    bytes[offset + 2] = 0xbb;
    bytes[offset + 3] = 0xcc;

    expect_true("36/1 offset", d64_track_sector_offset(36, 1, &offset) == D64_OK);
    bytes[offset] = 0;
    bytes[offset + 1] = 4;
    bytes[offset + 2] = 0x00;
    bytes[offset + 3] = 0xc0;
    bytes[offset + 4] = 0x60;

    expect_true(
        "mount 40-track extra-track PRGs",
        c64_mount_d64_ex(
            &machine,
            8,
            bytes,
            D64_40TRACK_IMAGE_SIZE,
            entries,
            2,
            "ext.d64",
            "TEST DISK",
            "ID",
            "2A",
            0,
            false) == C64_DRIVE_STATUS_OK);

    setup_load_call(&machine, "$", 8, 0);
    machine.cpu.cpu.X = 0x01;
    machine.cpu.cpu.Y = 0x08;
    expect_true("40-track directory load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_basic_contains(&machine, "EXTPRG");
    expect_basic_contains(&machine, "T36PRG");

    setup_load_call(&machine, "EXTPRG", 8, 1);
    expect_true("extra-track chain load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u8("chain byte from T18", 0xaa, c64_debug_read_ram(&machine, 0x0801));
    /* Intermediate D64 sectors always contribute 254 data bytes (2 of load address). */
    expect_u8("chain byte from T36", 0xbb, c64_debug_read_ram(&machine, 0x08fd));
    expect_u8("chain last byte from T36", 0xcc, c64_debug_read_ram(&machine, 0x08fe));

    setup_load_call(&machine, "T36PRG", 8, 1);
    expect_true("first_track=36 load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_u8("T36PRG byte", 0x60, c64_debug_read_ram(&machine, 0xc000));

    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_35track_trap_load_rejects_track_36(void) {
    c64_t machine;
    uint8_t *image;
    c64_drive_directory_entry entry;
    char error[256];

    reset_machine(&machine);
    image = (uint8_t *)calloc(1, C64_DRIVE_D64_STANDARD_SIZE);
    if (image == NULL) {
        fail("failed to allocate 35-track image");
    }
    memset(&entry, 0, sizeof(entry));
    entry.type = C64_DRIVE_FILE_PRG;
    entry.raw_type = 0x82;
    entry.first_track = 36;
    entry.first_sector = 0;
    put_fake_directory_name(&entry, "PASTEND");
    expect_true(
        "mount 35-track with T36 entry",
        c64_mount_d64(
            &machine,
            8,
            image,
            C64_DRIVE_D64_STANDARD_SIZE,
            &entry,
            1,
            "past.d64",
            "PAST",
            "  ",
            "  ",
            0) == C64_DRIVE_STATUS_OK);
    setup_load_call(&machine, "PASTEND", 8, 1);
    machine.bus.ram[0x3000] = 0x5a;
    expect_true("T36 on 35-track load", c64_step_instruction(&machine, error, sizeof(error)));
    expect_failure_return(&machine);
    expect_u8("T36 sentinel unchanged", 0x5a, machine.bus.ram[0x3000]);
    c64_unmount_all_drives(&machine);
    free(image);
}

static void test_40track_trap_save_preserves_extra_tracks(void) {
    c64_t machine;
    uint8_t *bytes;
    d64_image *saved_image;
    d64_directory_entry entry;
    d64_file_data file;
    d64_result result;
    char error[256];
    uint8_t expected[] = {0x01, 0x08, 0x11, 0x22, 0x33, 0x44};
    size_t i;

    reset_machine(&machine);
    bytes = make_blank_d64_of_size(D64_40TRACK_IMAGE_SIZE);
    if (bytes == NULL) {
        fail("failed to allocate blank 40-track image");
    }
    bytes[D64_STANDARD_IMAGE_SIZE] = 0x5a;
    bytes[D64_40TRACK_IMAGE_SIZE - 1u] = 0x3c;
    expect_true(
        "mount writable 40-track",
        c64_mount_d64_ex(
            &machine,
            8,
            bytes,
            D64_40TRACK_IMAGE_SIZE,
            NULL,
            0,
            "blank40.d64",
            "TEST DISK",
            "ID",
            "2A",
            664,
            true) == C64_DRIVE_STATUS_OK);
    for (i = 2; i < sizeof(expected); ++i) {
        machine.bus.ram[0x0801u + (uint16_t)(i - 2u)] = expected[i];
    }

    setup_save_call(&machine, "SAVED", 8, 0x0801, 0x0805);
    expect_true("40-track SAVE trap", c64_step_instruction(&machine, error, sizeof(error)));
    expect_success_return(&machine);
    expect_true("40-track SAVE dirty", machine.drives[0].dirty);
    expect_true(
        "SAVE keeps 40-track size",
        machine.drives[0].image_size == D64_40TRACK_IMAGE_SIZE);
    expect_u8(
        "SAVE left extra-track byte",
        0x5a,
        machine.drives[0].image_bytes[D64_STANDARD_IMAGE_SIZE]);
    expect_u8(
        "SAVE left last extra-track byte",
        0x3c,
        machine.drives[0].image_bytes[D64_40TRACK_IMAGE_SIZE - 1u]);

    saved_image = d64_image_create(
        machine.drives[0].image_bytes, machine.drives[0].image_size, &result);
    expect_true("saved 40-track parses", saved_image != NULL && result == D64_OK);
    expect_true(
        "find 40-track saved file",
        d64_image_find_entry_ascii(saved_image, "SAVED", &entry) == D64_OK);
    memset(&file, 0, sizeof(file));
    expect_true(
        "extract 40-track saved file",
        d64_image_extract_prg(saved_image, &entry, &file) == D64_OK);
    expect_u16("40-track saved size", (uint16_t)sizeof(expected), (uint16_t)file.size);
    if (file.size != sizeof(expected) || memcmp(file.bytes, expected, sizeof(expected)) != 0) {
        fail("40-track saved PRG bytes mismatch");
    }

    d64_file_data_free(&file);
    d64_image_destroy(saved_image);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

static void test_error_info_mount_preserves_tail(void) {
    c64_t machine;
    uint8_t *bytes;
    c64_drive_status_result status;

    reset_machine(&machine);
    bytes = (uint8_t *)calloc(1, D64_ERROR_INFO_IMAGE_SIZE);
    if (bytes == NULL) {
        fail("failed to allocate error-info image");
    }
    bytes[D64_STANDARD_IMAGE_SIZE - 1u] = 0xa5;
    bytes[D64_STANDARD_IMAGE_SIZE] = 0x5a;
    bytes[D64_ERROR_INFO_IMAGE_SIZE - 1u] = 0x3c;
    status = c64_mount_d64_ex(
        &machine,
        8,
        bytes,
        D64_ERROR_INFO_IMAGE_SIZE,
        NULL,
        0,
        "err.d64",
        "",
        "",
        "",
        0,
        false);
    expect_true("175531 mount succeeds", status == C64_DRIVE_STATUS_OK);
    expect_true(
        "175531 image_size",
        machine.drives[0].image_size == D64_ERROR_INFO_IMAGE_SIZE);
    expect_u8(
        "payload last byte kept",
        0xa5,
        machine.drives[0].image_bytes[D64_STANDARD_IMAGE_SIZE - 1u]);
    expect_u8(
        "error tail first byte kept",
        0x5a,
        machine.drives[0].image_bytes[D64_STANDARD_IMAGE_SIZE]);
    expect_u8(
        "error tail last byte kept",
        0x3c,
        machine.drives[0].image_bytes[D64_ERROR_INFO_IMAGE_SIZE - 1u]);
    free(bytes);
    c64_unmount_all_drives(&machine);
}

int main(void) {
    test_canonical_d64_sizes_mount_full_blob();
    test_40track_mount_does_not_truncate();
    test_40track_trap_load_extra_track_chain();
    test_35track_trap_load_rejects_track_36();
    test_40track_trap_save_preserves_extra_tracks();
    test_error_info_mount_preserves_tail();
    {
        char asset_path[512];
        snprintf(asset_path, sizeof(asset_path),
                 "%s/assets/c64/disks/ODELLLAK.D64", C64M_SOURCE_DIR);
        if (c64m_test_asset_missing(asset_path)) {
            return C64M_TEST_SKIP;
        }
        snprintf(asset_path, sizeof(asset_path),
                 "%s/assets/c64/disks/blank.d64", C64M_SOURCE_DIR);
        if (c64m_test_asset_missing(asset_path)) {
            return C64M_TEST_SKIP;
        }
    }
    test_load_prg_secondary_one();
    test_load_prg_basic_semantics();
    test_failures_and_device_fallthrough();
    test_directory_load_blank_and_odell();
    test_wildcard_and_case_matching();
    test_malformed_mounted_chains_fail_safely();
    test_device_9_loads_and_independent_slots();
    test_kernal_save_trap_writes_prg();
    return 0;
}
