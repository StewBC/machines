#include "c1541.h"
#include "c64.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* ROM intercept constants                                             */
/* Standard 1541 ROM (DOS 2.6: 325302-01 + 901229-06AA combined).    */
/* Addresses verified against: github.com/mist64/1541rom (dos1541    */
/* source, 325302-01 / 901229-06 variant, labels from dos.lbl).      */
/*                                                                     */
/* With emulate_1541, GCR media is always on: READ/SEARCH/VERIFY/EXECUTE run   */
/* the ROM against the rotating track. D64 WRITE stays a hybrid job intercept  */
/* (persist sector bytes, then poke GCR) so BAM/directory SAVEs stay coherent. */
/* ------------------------------------------------------------------ */
#define C1541_ZP_JOBS        0x0000u  /* jobs[0..5]: job queue (6 bytes) */
#define C1541_ZP_HDRS        0x0006u  /* hdrs[0..11]: track/sector pairs */
#define C1541_RAM_SECTOR_BUF 0x0300u  /* buff0; buff_N = $0300 + N*$0100 */

#define C1541_JOB_CMD_MASK   0x78u
#define C1541_JOB_CMD_WRITE  0x10u    /* WRITE job ($90 & MASK); dos.lbl "WRITE" */

#define C1541_JOB_OK         0x01u    /* job result: success */
#define C1541_JOB_ERROR      0x02u    /* job result: generic error */
#define C1541_JOB_WRITE_PROT 0x08u    /* job result: write protect on (DOS 26) */

/* Maximum valid job index (jobs 0–4 have dedicated buffers at $0300–$0700).
   Job 5 shares page $07 with job 4 and is never a READ job (command channel). */
#define C1541_JOB_MAX        5u

/* ------------------------------------------------------------------ */
/* D64 sector offset (inline table — avoids tools/d64/ dependency)    */
/* ------------------------------------------------------------------ */

/* Returns byte offset of (track, sector) in a standard 35-track D64 image.
   Track is 1-based; sector is 0-based.  Returns -1 for out-of-range inputs. */
static int d64_sector_offset(uint8_t track, uint8_t sector) {
    static const uint8_t spt[36] = {
        0,
        21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
        19,19,19,19,19,19,19,
        18,18,18,18,18,18,
        17,17,17,17,17
    };
    int offset;
    int t;

    if (track < 1 || track > 35) return -1;
    if (sector >= spt[track]) return -1;
    offset = 0;
    for (t = 1; t < (int)track; t++) offset += (int)spt[t] * 256;
    offset += (int)sector * 256;
    return offset;
}

/* Number of sectors on a standard 35-track D64 track, or -1 if out of range. */
static int d64_sectors_per_track(uint8_t track) {
    if (track < 1 || track > 35) return -1;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

/* ------------------------------------------------------------------ */
/* Bus callbacks (static — not exposed)                                */
/* ------------------------------------------------------------------ */

static void c1541_update_iec_bus(c1541 *drive);

static uint8_t c1541_bus_read(void *user, uint16_t addr) {
    c1541 *drive = (c1541 *)user;
    if (addr < 0x0800u) return drive->ram[addr];
    if (addr < 0x1000u) return drive->ram[addr & 0x07FFu];
    /* Check VIA #2 before VIA #1 — $1C00–$1FFF is a subset of $1800–$1FFF */
    if (addr >= 0x1C00u && addr < 0x2000u) {
        uint8_t reg = (uint8_t)(addr & 0x0Fu);
        uint8_t value = via6522_read(&drive->via2, reg);
        /* Port A ($1C01 / no-handshake $1C0F): consuming a GCR byte clears BYTE READY. */
        if (reg == 0x01u || reg == 0x0Fu) {
            c1541_media_on_port_a_read(drive);
        }
        return value;
    }
    if (addr >= 0x1800u && addr < 0x2000u) {
        /* Refresh IEC inputs before the drive samples Port B (bitbang waits). */
        c1541_update_iec_bus(drive);
        return via6522_read(&drive->via1, (uint8_t)(addr & 0x0Fu));
    }
    if (addr >= 0xC000u) return drive->rom[addr - 0xC000u];
    return 0xFFu;
}

static void c1541_bus_write(void *user, uint16_t addr, uint8_t value) {
    c1541 *drive = (c1541 *)user;
    if (addr < 0x0800u) { drive->ram[addr] = value; return; }
    if (addr < 0x1000u) { drive->ram[addr & 0x07FFu] = value; return; }
    if (addr >= 0x1C00u && addr < 0x2000u) {
        uint8_t reg = (uint8_t)(addr & 0x0Fu);
        via6522_write(&drive->via2, reg, value);
        /* Port A write supplies the next GCR byte while the head is writing. */
        if (reg == 0x01u || reg == 0x0Fu) {
            c1541_media_on_port_a_write(drive, value);
        }
        return;
    }
    if (addr >= 0x1800u && addr < 0x2000u) {
        uint8_t reg = (uint8_t)(addr & 0x0Fu);
        via6522_write(&drive->via1, reg, value);
        return;
    }
    /* ROM and unmapped: ignore */
}

/* ------------------------------------------------------------------ */
/* IRQ / NMI callbacks                                                 */
/* ------------------------------------------------------------------ */

static uint8_t c1541_irq_pending(void *user) {
    c1541 *drive = (c1541 *)user;
    uint16_t pc;

    /* Custom drive code in RAM often runs with I=0. VIA2 T1 free-runs for GCR
       byte-ready; its IRQ enters ROM $F2B0 which, when the job queue is idle,
       JMP $F99C instead of RTS — abandoning the custom routine (Robocop stage-3
       $0371 checksum; C64 ACPTR then reads garbage). Ack T1 while PC is in RAM
       and no job is active; keep T1 when a job is pending so GCR can complete.
       VIA1 (ATN) is never filtered. */
    pc = drive->cpu.cpu.pc;
    if (pc < 0xC000u && (drive->via2.ifr & drive->via2.ier & 0x40u) != 0) {
        int job_active = 0;
        uint8_t n;
        for (n = 0; n < 6u; n++) {
            if ((drive->ram[C1541_ZP_JOBS + n] & 0x80u) != 0) {
                job_active = 1;
                break;
            }
        }
        if (!job_active) {
            drive->via2.ifr &= (uint8_t)~0x40u;
        }
    }

    return (uint8_t)(via6522_irq_pending(&drive->via1) || via6522_irq_pending(&drive->via2));
}

/* ------------------------------------------------------------------ */
/* IEC bus synchronisation                                             */
/* ------------------------------------------------------------------ */

/* Called once per cycle after VIA steps and before CPU step.
   Reads VIA #1 Port B output lines, updates the C64's iec_external_pull,
   then reads the combined bus state and feeds it back into VIA #1
   input pins and CA1 (ATN IRQ source). */
/* Map serial VIA ORB/DDRB (+ ATN state) to open-collector IEC pull bits. */
static uint8_t c1541_iec_pull_from_orb(uint8_t ddrb, uint8_t orb, int atn_low) {
    uint8_t drive_pull = 0;

    /* Standard 1541 serial VIA ($1800) Port B:
       bit 0 = DATA in, bit 1 = DATA out, bit 2 = CLK in, bit 3 = CLK out,
       bit 4 = ATN acknowledge control, bits 5/6 = address jumpers, bit 7 = ATN in.
       The serial VIA is behind inverters:
       output bit high pulls the IEC line low, and input bit high means the
       IEC line is currently low.  PB4 is special: when ATN is asserted and
       PB4 is output-low, the drive acknowledges attention by pulling DATA low. */
    if ((ddrb & 0x02u) && (orb & 0x02u)) {
        drive_pull |= C64_IEC_DATA;
    }
    if ((ddrb & 0x10u) && !(orb & 0x10u) && atn_low) {
        drive_pull |= C64_IEC_DATA;
    }
    if ((ddrb & 0x08u) && (orb & 0x08u)) {
        drive_pull |= C64_IEC_CLK;
    }
    return drive_pull;
}

static void c1541_update_iec_bus(c1541 *drive) {
    uint8_t drive_pull_ext;
    uint8_t drive_pull_self;
    uint8_t c64_pull, bus_low;
    uint8_t data_low, clk_low, atn_low;
    uint8_t port_b_in;
    int atn_from_c64;

    c64_pull = c64_get_iec_c64_pull(drive->c64);
    atn_from_c64 = (c64_pull & C64_IEC_ATN) != 0;

    /* Peers (C64 / other drives) see the drive's current VIA output. VICE catches
       the drive up to the exact C64 clock on each CIA2 IEC access and resolves the
       bus with no fixed pipeline delay; mirror that by exposing immediate output. */
    drive_pull_ext = c1541_iec_pull_from_orb(
        drive->via1.ddrb, drive->via1.orb, atn_from_c64);
    c64_set_iec_drive_pull(drive->c64, drive->device_number, drive_pull_ext);

    /* Local $1800 sense uses immediate VIA ORB/DDRB for this drive's contribution.
       A settle delay on self-pull makes bitbang "wait for host" loops (LDX $1800 /
       BEQ) see their own previous output as a false handshake. */
    drive_pull_self = c1541_iec_pull_from_orb(
        drive->via1.ddrb, drive->via1.orb, atn_from_c64);
    bus_low = (uint8_t)(c64_get_iec_pull_excluding_drive(drive->c64, drive->device_number) |
                        drive_pull_self | c64_pull);

    data_low = (bus_low & C64_IEC_DATA) != 0;
    clk_low  = (bus_low & C64_IEC_CLK)  != 0;
    atn_low  = (bus_low & C64_IEC_ATN)  != 0;

    /* Feed combined bus state into VIA #1 Port B input pins.
       Bus line low (asserted) → input bit = 1 due the 1541 input inverters. */
    port_b_in = drive->via1.port_b_in;
    if (data_low) port_b_in |= 0x01u; else port_b_in &= (uint8_t)~0x01u;
    if (clk_low)  port_b_in |= 0x04u; else port_b_in &= (uint8_t)~0x04u;
    if (atn_low)  port_b_in |= 0x80u; else port_b_in &= (uint8_t)~0x80u;
    via6522_set_port_b_inputs(&drive->via1, port_b_in);

    /* ATN → CA1 through the same input polarity: asserted ATN presents high. */
    via6522_set_ca1(&drive->via1, atn_low ? 1u : 0u);
}

/* ------------------------------------------------------------------ */
/* D64 hybrid WRITE intercept (READ/SEARCH/VERIFY/EXECUTE use GCR ROM) */
/* ------------------------------------------------------------------ */

static int c1541_job_sector_offset(c1541 *drive, uint8_t n, int *out_offset) {
    uint8_t track, sector;
    const c64_drive_slot *slot;
    int offset;

    /* Track and sector from hdrs[n*2] and hdrs[n*2+1] (ZP $06 + n*2). */
    track  = drive->ram[C1541_ZP_HDRS + (uint16_t)n * 2u];
    sector = drive->ram[C1541_ZP_HDRS + (uint16_t)n * 2u + 1u];

    slot = c64_get_drive_slot(drive->c64, drive->device_number);
    if (!slot || !slot->mounted || !slot->image_bytes || slot->image_size == 0) {
        return 0;
    }

    offset = d64_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > slot->image_size) {
        return 0;
    }
    *out_offset = offset;
    return 1;
}

/* Writes the active job's buffer back into the mounted D64 image.
   DOS fills the buffer before queuing the WRITE job, so at the job-dispatch
   window the buffer already holds the 256 bytes to persist.
   Returns C1541_JOB_OK on success, C1541_JOB_WRITE_PROT if the image is mounted
   read-only, or C1541_JOB_ERROR if the sector is out of range / not mounted.
   Marks the slot dirty on success; the runtime flushes it to the host .d64. */
static uint8_t c1541_copy_job_buffer_to_sector(c1541 *drive, uint8_t n) {
    c64_drive_slot *slot;
    int offset;
    uint16_t buf_addr;

    if (!c1541_job_sector_offset(drive, n, &offset)) {
        return C1541_JOB_ERROR;
    }
    slot = c64_get_drive_slot_mut(drive->c64, drive->device_number);
    if (!slot) {
        return C1541_JOB_ERROR;
    }
    if (!slot->writable) {
        return C1541_JOB_WRITE_PROT;
    }
    if (drive->c64 != NULL && drive->c64->replay_sealed) {
        return C1541_JOB_OK;
    }

    buf_addr = (uint16_t)(C1541_RAM_SECTOR_BUF + (uint16_t)n * 0x0100u);
    memcpy(slot->image_bytes + offset, &drive->ram[buf_addr], 256);
    slot->dirty = true;
    slot->image_content_seq++;
    c64_notify_guest_media_write(drive->c64, drive->device_number);
    return C1541_JOB_OK;
}

static void c1541_complete_queued_job(c1541 *drive, uint8_t n, uint8_t result) {
    drive->ram[C1541_ZP_JOBS + n] = result;
}

static void c1541_pulse_write_led(c1541 *drive) {
    if (drive != NULL && drive->c64 != NULL) {
        c64_disk_activity_write(drive->c64, drive->device_number);
    }
}

static int c1541_satisfy_queued_job(c1541 *drive, uint8_t n) {
    uint8_t raw, command;

    raw = drive->ram[C1541_ZP_JOBS + n];
    if ((raw & 0x80u) == 0 || raw == 0xD0u) {
        return 0;
    }

    command = (uint8_t)(raw & C1541_JOB_CMD_MASK);
    if (command != C1541_JOB_CMD_WRITE) {
        /* READ/SEARCH/VERIFY/EXECUTE: ROM + GCR media path. */
        return 0;
    }

    {
        /* D64: hybrid write — persist job buffer, then poke GCR when tracks live.
           G64: no sector map; ROM Port-A when writable+media, else write-protect. */
        uint8_t wr;
        uint8_t trk, sec;
        uint16_t buf_addr;
        const c64_drive_slot *slot;

        slot = c64_get_drive_slot(drive->c64, drive->device_number);
        if (slot != NULL && slot->image_kind == C64_DRIVE_IMAGE_G64) {
            if (c1541_media_physical_write_active(drive) && slot->writable) {
                return 0; /* ROM physical write */
            }
            c1541_complete_queued_job(drive, n, C1541_JOB_WRITE_PROT);
            return 1;
        }

        wr = c1541_copy_job_buffer_to_sector(drive, n);
        if (wr == C1541_JOB_OK && c1541_media_physical_write_active(drive)) {
            trk = drive->ram[C1541_ZP_HDRS + (uint16_t)n * 2u];
            sec = drive->ram[C1541_ZP_HDRS + (uint16_t)n * 2u + 1u];
            buf_addr = (uint16_t)(C1541_RAM_SECTOR_BUF + (uint16_t)n * 0x0100u);
            if (!c1541_media_poke_sector(drive, trk, sec, &drive->ram[buf_addr])) {
                /* D64 advanced; GCR stale until rebuild. */
                c1541_media_invalidate(&drive->media);
            }
        } else if (wr == C1541_JOB_OK) {
            /* Tracks not live yet: drop cache so the next media step rebuilds. */
            c1541_media_invalidate(&drive->media);
        }
        if (wr == C1541_JOB_OK) {
            c1541_pulse_write_led(drive);
        }
        c1541_complete_queued_job(drive, n, wr);
        return 1;
    }
}

static void c1541_satisfy_queued_jobs(c1541 *drive) {
    uint8_t n;

    for (n = 0; n < C1541_JOB_MAX; ++n) {
        (void)c1541_satisfy_queued_job(drive, n);
    }
}

static void c1541_update_so(c1541 *drive) {
    /* In media mode BYTE READY drives SO from c1541_media_step.  Do not also
       pulse SO from VIA2 T1 PB7 free-run — that desynchronises the ROM's BVC
       wait-for-byte loops. */
    if (drive->media.enabled && drive->media.tracks_valid) {
        drive->via2_t1_pb7_last = drive->via2.t1_pb7_state;
        return;
    }
    if (drive->via2.t1_pb7_state != drive->via2_t1_pb7_last) {
        drive->via2_t1_pb7_last = drive->via2.t1_pb7_state;
        c6510_set_overflow(&drive->cpu);
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void c1541_init(c1541 *drive, c64_t *c64, int device_number) {
    memset(drive, 0, sizeof(c1541));
    drive->c64 = c64;
    drive->device_number = device_number;
    via6522_init(&drive->via1);
    via6522_init(&drive->via2);
    c1541_media_init(&drive->media);
    c6510_init(&drive->cpu, drive, c1541_bus_read, c1541_bus_write);
    c6510_set_irq_pending_callback(&drive->cpu, c1541_irq_pending);
}

void c1541_rewire(c1541 *drive, c64_t *c64) {
    drive->c64 = c64;
    drive->cpu.user = drive;
    drive->cpu.read = c1541_bus_read;
    drive->cpu.write = c1541_bus_write;
    c6510_set_irq_pending_callback(&drive->cpu, c1541_irq_pending);
}

void c1541_destroy(c1541 *drive) {
    c1541_media_free_tracks(&drive->media);
    memset(drive, 0, sizeof(c1541));
}

void c1541_reset(c1541 *drive) {
    memset(drive->ram, 0, C1541_RAM_SIZE);
    via6522_reset(&drive->via1);
    via6522_reset(&drive->via2);
    c1541_media_reset(&drive->media);
    /* Device address jumpers are serial VIA ($1800) Port B bits 5/6.
       The DOS ROM converts 00→device 8, 20→device 9, 40→device 10, 60→device 11. */
    if (drive->device_number == 9) {
        drive->via1.port_b_in |= 0x20u;
    }
    /* c6510_reset() reads $FFFC/$FFFD through the bus callbacks.
       With ROM loaded at $C000–$FFFF this correctly fetches the 1541 reset vector. */
    c6510_reset(&drive->cpu);
    drive->cpu_cycles_remaining = 0;
    drive->via2_t1_pb7_last = drive->via2.t1_pb7_state;
}

/* ------------------------------------------------------------------ */
/* ROM loading                                                         */
/* ------------------------------------------------------------------ */

int c1541_load_rom(c1541 *drive, const char *path) {
    FILE *f;
    size_t n;

    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(drive->rom, 1, C1541_ROM_SIZE, f);
    fclose(f);
    if (n != C1541_ROM_SIZE) {
        memset(drive->rom, 0, C1541_ROM_SIZE);
        return 0;
    }
    drive->rom_loaded = 1;
    return 1;
}

int c1541_load_rom_split(c1541 *drive, const char *path_lo, const char *path_hi) {
    FILE *f;
    size_t n;

    f = fopen(path_lo, "rb");
    if (!f) return 0;
    n = fread(drive->rom, 1, C1541_ROM_SIZE / 2, f);
    fclose(f);
    if (n != (size_t)(C1541_ROM_SIZE / 2)) {
        memset(drive->rom, 0, C1541_ROM_SIZE);
        return 0;
    }

    f = fopen(path_hi, "rb");
    if (!f) { memset(drive->rom, 0, C1541_ROM_SIZE); return 0; }
    n = fread(drive->rom + C1541_ROM_SIZE / 2, 1, C1541_ROM_SIZE / 2, f);
    fclose(f);
    if (n != (size_t)(C1541_ROM_SIZE / 2)) {
        memset(drive->rom, 0, C1541_ROM_SIZE);
        return 0;
    }

    drive->rom_loaded = 1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Cycle step                                                          */
/* ------------------------------------------------------------------ */

/* Peek opcode at PC without VIA/media side effects (ROM/RAM only). */
static uint8_t c1541_peek_opcode(const c1541 *drive) {
    uint16_t pc = drive->cpu.cpu.pc;

    if (pc < 0x0800u) {
        return drive->ram[pc];
    }
    if (pc < 0x1000u) {
        return drive->ram[pc & 0x07FFu];
    }
    if (pc >= 0xC000u && drive->rom_loaded) {
        return drive->rom[pc - 0xC000u];
    }
    return 0xEAu; /* treat unmapped as NOP for can_begin */
}

/*
 * Drive 6502 SO / BYTE READY — match VICE drivecpu + rotation notes:
 *
 *  - BYTE READY is a sticky edge (so_pulse).
 *  - BVC / BVS / PHP sample the edge mid-instruction (VICE 6510core.c) so the
 *    branch / status push sees V in the same instruction.
 *  - CLV clears V and discards any pending edge (VICE LOCAL_SET_OVERFLOW(0)).
 *    Without that, dual-BVC loaders re-read the same GCR byte ($2842 fill).
 *  - Any edge not consumed mid-instruction is applied after Phi2 (SO pin).
 */
static void c1541_so_take_edge(c1541 *drive) {
    if (!drive->media.enabled || !drive->media.so_pulse) {
        return;
    }
    drive->media.so_pulse = 0;
    c6510_set_overflow(&drive->cpu);
}

static void c1541_micro_step_with_so(c1541 *drive) {
    uint8_t op;
    uint8_t phase;
    int finished;

    if (!drive->cpu.micro_active) {
        return;
    }

    op = drive->cpu.micro_opcode;
    phase = drive->cpu.micro_phase;

    /* Sample SO before BVC/BVS decide the branch, and before PHP pushes P. */
    if (phase == 1u &&
        (op == (uint8_t)BVC_rel || op == (uint8_t)BVS_rel || op == (uint8_t)PHP)) {
        c1541_so_take_edge(drive);
    }

    finished = c6510_micro_step(&drive->cpu) ? 1 : 0;

    /* micro_opcode remains valid after finish until the next micro_begin. */
    if (finished && drive->cpu.micro_opcode == (uint8_t)CLV && drive->media.enabled) {
        drive->media.so_pulse = 0;
    }
}

static void c1541_run_cpu_one_cycle(c1541 *drive) {
    size_t cycles;
    c6510_interrupt_kind interrupt_kind;
    uint8_t opcode;
    uint16_t pc_before;

    /* Prefer Phi2 micro-step so multi-cycle STA/STX $1800 write on the last
       cycle (needed for IEC bitbang). Every documented NMOS opcode is covered;
       only JAM/unstable undocs fall back to bulk instruction step. */
    if (drive->cpu.micro_active) {
        c1541_micro_step_with_so(drive);
        c1541_update_iec_bus(drive);
        return;
    }

    if (drive->cpu_cycles_remaining > 0u) {
        drive->cpu_cycles_remaining--;
        return;
    }

    pc_before = drive->cpu.cpu.pc;
    (void)pc_before;

    /* Hybrid D64 WRITE (and G64 write-protect) at the DOS job-scan window. */
    if (drive->cpu.cpu.pc >= 0xF2B0u && drive->cpu.cpu.pc <= 0xF2F6u) {
        c1541_satisfy_queued_jobs(drive);
    }

    interrupt_kind = c6510_micro_poll_interrupt(&drive->cpu);
    if (interrupt_kind != C6510_INTERRUPT_NONE) {
        c6510_micro_begin_interrupt(&drive->cpu, interrupt_kind);
        c1541_micro_step_with_so(drive);
        c1541_update_iec_bus(drive);
        return;
    }

    opcode = c1541_peek_opcode(drive);
    if (c6510_micro_can_begin(&drive->cpu, opcode)) {
        c6510_micro_begin(&drive->cpu);
        c1541_micro_step_with_so(drive);
        c1541_update_iec_bus(drive);
        return;
    }

    /* Bulk fallback (JAM / unstable undocs only for normal 1541 code). */
    cycles = c6510_step(&drive->cpu);
    drive->cpu_cycles_remaining = cycles != 0 ? cycles : 1u;
    c1541_update_iec_bus(drive);
    drive->cpu_cycles_remaining--;
}

void c1541_advance_one_cycle(c1541 *drive) {
    if (!drive->rom_loaded) return;

    /* GCR media follows emulate_1541 (full 1541 path). */
    if (drive->c64 != NULL) {
        drive->media.enabled = drive->c64->config.emulate_1541 != 0 ? 1 : 0;
    }

    /* 1. Step VIAs (may set IFR flags before CPU samples them). */
    via6522_step(&drive->via1);
    via6522_step(&drive->via2);

    /* 2. Disk-controller media (no-op when emulate_1541 is off). */
    if (drive->media.enabled) {
        c1541_media_step(drive);
    }

    /* 3. Timer1 PB7→SO remains for non-media / IEC bit-timing paths. */
    c1541_update_so(drive);

    /* 4. Synchronise IEC bus: serial VIA outputs → C64 pull; C64 state → serial VIA inputs + CA1. */
    c1541_update_iec_bus(drive);

    /* 5. Drive CPU: one Phi2 cycle (micro path preferred; may write $1800). */
    c1541_run_cpu_one_cycle(drive);

    /* 6. Remaining BYTE READY edge → SO after Phi2 (if not already taken by
       BVC/BVS/PHP, and not discarded by CLV). */
    if (drive->media.enabled && drive->media.so_pulse) {
        drive->media.so_pulse = 0;
        c6510_set_overflow(&drive->cpu);
    }
}

int c1541_debug_read_map(const c1541 *drive, uint16_t address, uint8_t *out_value) {
    if (drive == NULL || out_value == NULL) {
        return 0;
    }

    if (address < 0x0800u) {
        *out_value = drive->ram[address];
        return 1;
    }
    if (address < 0x1000u) {
        *out_value = drive->ram[address & 0x07FFu];
        return 1;
    }
    if (address >= 0x1C00u && address < 0x2000u) {
        *out_value = via6522_debug_read_register(&drive->via2, (uint8_t)(address & 0x0Fu));
        return 1;
    }
    if (address >= 0x1800u && address < 0x2000u) {
        *out_value = via6522_debug_read_register(&drive->via1, (uint8_t)(address & 0x0Fu));
        return 1;
    }
    if (address >= 0xC000u && drive->rom_loaded) {
        *out_value = drive->rom[address - 0xC000u];
        return 1;
    }

    *out_value = 0;
    return 0;
}
