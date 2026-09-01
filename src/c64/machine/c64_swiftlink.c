#include "c64_swiftlink.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef enum response_id {
    RESP_OK = 0,
    RESP_CONNECT,
    RESP_NO_CARRIER,
    RESP_ERROR,
    RESP_NO_DIALTONE,
    RESP_NO_ANSWER
} response_id;

static uint16_t normalize_base(uint16_t base) {
    if (base == C64_SWIFTLINK_BASE_DF00) {
        return C64_SWIFTLINK_BASE_DF00;
    }
    return C64_SWIFTLINK_BASE_DE00;
}

static void clear_rings(c64_swiftlink *sl) {
    sl->tx_head = 0;
    sl->tx_tail = 0;
    sl->tx_count = 0;
    sl->rx_head = 0;
    sl->rx_tail = 0;
    sl->rx_count = 0;
}

static void clear_escape(c64_swiftlink *sl) {
    sl->escape_len = 0;
    sl->escape_flushing = false;
    sl->escape_flush_pos = 0;
    sl->escape_abort_byte = 0;
    sl->escape_after_guard = false;
}

static void modem_defaults(c64_swiftlink *sl) {
    sl->mode = C64_SWIFTLINK_MODE_COMMAND;
    sl->echo = true;
    sl->verbose = true;
    sl->at_len = 0;
    sl->ignore_lf = false;
    sl->at_overflow = false;
    clear_escape(sl);
}

static void cold_acia(c64_swiftlink *sl) {
    sl->command = 0;
    sl->control = 0;
    sl->turbo232 = 0;
    sl->tx_holding = 0;
    sl->tx_holding_full = false;
    sl->rx_holding = 0;
    sl->rx_holding_full = false;
    sl->carrier_present = false;
    sl->overrun = false;
    sl->irq_latched = false;
    sl->prev_rdrf = false;
    sl->prev_tdre = true; /* TDRE idle after reset */
    clear_rings(sl);
    modem_defaults(sl);
}

static uint64_t guard_cycles(const c64_swiftlink *sl) {
    uint32_t hz = sl->time_hz != 0u ? sl->time_hz : 985248u;
    return (uint64_t)hz;
}

static bool quiet_before_ok(const c64_swiftlink *sl) {
    return (sl->time_cycle - sl->last_tx_cycle) >= guard_cycles(sl);
}

static bool tx_irq_enabled(const c64_swiftlink *sl) {
    /* Command bits 3-2 == 01 and DTR (bit 0) set. */
    return (sl->command & 0x01u) != 0u && ((sl->command >> 2) & 0x03u) == 0x01u;
}

static bool rx_irq_enabled(const c64_swiftlink *sl) {
    /* DTR on and receiver IRQ not disabled (bit 1 == 0). */
    return (sl->command & 0x01u) != 0u && (sl->command & 0x02u) == 0u;
}

static void note_tx_activity(c64_swiftlink *sl) {
    sl->last_tx_cycle = sl->time_cycle;
}

static void update_irq_latch(c64_swiftlink *sl) {
    bool rdrf = sl->rx_holding_full;
    bool tdre = !sl->tx_holding_full;

    if (rx_irq_enabled(sl) && rdrf && !sl->prev_rdrf) {
        sl->irq_latched = true;
    }
    if (tx_irq_enabled(sl) && tdre && !sl->prev_tdre) {
        sl->irq_latched = true;
    }
    if (sl->overrun && rx_irq_enabled(sl)) {
        sl->irq_latched = true;
    }
    sl->prev_rdrf = rdrf;
    sl->prev_tdre = tdre;
}

uint32_t c64_swiftlink_bps(const c64_swiftlink *sl) {
    /* 6551 internal table; SwiftLink/Turbo232 doubles non-enhanced rates.
       Enhanced (control baud nibble 0) uses Turbo232 $xx07 bits 1-0. */
    static const uint32_t k_base[16] = {
        0u, 50u, 75u, 110u, 135u, 150u, 300u, 600u,
        1200u, 1800u, 2400u, 3600u, 4800u, 7200u, 9600u, 19200u
    };
    static const uint32_t k_enh[4] = { 230400u, 115200u, 57600u, 28800u };
    uint8_t nibble;

    assert(sl);
    nibble = (uint8_t)(sl->control & 0x0Fu);
    if (nibble == 0u) {
        return k_enh[sl->turbo232 & 0x03u];
    }
    return k_base[nibble] * 2u;
}

static uint64_t cycles_per_byte(const c64_swiftlink *sl) {
    uint32_t bps = c64_swiftlink_bps(sl);
    uint32_t hz = sl->time_hz != 0u ? sl->time_hz : 985248u;
    if (bps == 0u) {
        return 1u;
    }
    /* 8N1 => 10 bit-times per byte. */
    return ((uint64_t)hz * 10ull + (uint64_t)bps - 1ull) / (uint64_t)bps;
}

static bool tx_pace_ready(const c64_swiftlink *sl) {
    if (!sl->pace_baud) {
        return true;
    }
    return (sl->time_cycle - sl->last_tx_bit_cycle) >= cycles_per_byte(sl);
}

static bool rx_pace_ready(const c64_swiftlink *sl) {
    if (!sl->pace_baud) {
        return true;
    }
    return (sl->time_cycle - sl->last_rx_bit_cycle) >= cycles_per_byte(sl);
}

static bool tx_ring_push(c64_swiftlink *sl, uint8_t byte) {
    if (sl->tx_count >= C64_SWIFTLINK_TX_RING_SIZE) {
        return false;
    }
    sl->tx_ring[sl->tx_head] = byte;
    sl->tx_head = (uint16_t)((sl->tx_head + 1u) % C64_SWIFTLINK_TX_RING_SIZE);
    sl->tx_count++;
    return true;
}

static bool rx_ring_push(c64_swiftlink *sl, uint8_t byte) {
    if (sl->rx_count >= C64_SWIFTLINK_RX_RING_SIZE) {
        return false;
    }
    sl->rx_ring[sl->rx_head] = byte;
    sl->rx_head = (uint16_t)((sl->rx_head + 1u) % C64_SWIFTLINK_RX_RING_SIZE);
    sl->rx_count++;
    return true;
}

static bool rx_ring_pop(c64_swiftlink *sl, uint8_t *out) {
    if (sl->rx_count == 0) {
        return false;
    }
    *out = sl->rx_ring[sl->rx_tail];
    sl->rx_tail = (uint16_t)((sl->rx_tail + 1u) % C64_SWIFTLINK_RX_RING_SIZE);
    sl->rx_count--;
    return true;
}

static uint8_t status_byte(const c64_swiftlink *sl) {
    uint8_t status = 0;

    if (!sl->carrier_present) {
        status |= C64_SWIFTLINK_STATUS_CD;
    }
    if (!sl->enabled) {
        status |= C64_SWIFTLINK_STATUS_DSR;
    }
    if (!sl->tx_holding_full) {
        status |= C64_SWIFTLINK_STATUS_TDRE;
    }
    if (sl->rx_holding_full) {
        status |= C64_SWIFTLINK_STATUS_RDRF;
    }
    if (sl->overrun) {
        status |= C64_SWIFTLINK_STATUS_OVERRUN;
    }
    if (sl->irq_latched) {
        status |= C64_SWIFTLINK_STATUS_IRQ;
    }
    return status;
}

static uint8_t turbo232_read(const c64_swiftlink *sl) {
    uint8_t value = (uint8_t)(sl->turbo232 & 0x03u);
    if ((sl->control & 0x0Fu) == 0) {
        value |= 0x04u;
    }
    return value;
}

static void latch_hangup_req(c64_swiftlink *sl) {
    /* CONNECT superseded by hangup; hangup wins if both were pending. */
    memset(&sl->pending_req, 0, sizeof(sl->pending_req));
    sl->pending_req.kind = C64_SWIFTLINK_HOST_REQ_HANGUP;
}

static void latch_connect_req(c64_swiftlink *sl, const char *host, uint16_t port) {
    size_t n;

    memset(&sl->pending_req, 0, sizeof(sl->pending_req));
    sl->pending_req.kind = C64_SWIFTLINK_HOST_REQ_CONNECT;
    sl->pending_req.port = port;
    n = strlen(host);
    if (n >= C64_SWIFTLINK_HOST_MAX) {
        n = C64_SWIFTLINK_HOST_MAX - 1u;
    }
    memcpy(sl->pending_req.host, host, n);
    sl->pending_req.host[n] = '\0';
}

static void enqueue_response(c64_swiftlink *sl, response_id id) {
    const char *text;
    char numeric;
    const char *p;

    if (sl->verbose) {
        switch (id) {
        case RESP_OK:
            text = "OK";
            break;
        case RESP_CONNECT:
            text = "CONNECT";
            break;
        case RESP_NO_CARRIER:
            text = "NO CARRIER";
            break;
        case RESP_ERROR:
            text = "ERROR";
            break;
        case RESP_NO_DIALTONE:
            text = "NO DIALTONE";
            break;
        case RESP_NO_ANSWER:
            text = "NO ANSWER";
            break;
        default:
            text = "ERROR";
            break;
        }
        for (p = text; *p != '\0'; ++p) {
            if (!rx_ring_push(sl, (uint8_t)*p)) {
                sl->overrun = true;
                return;
            }
        }
    } else {
        switch (id) {
        case RESP_OK:
            numeric = '0';
            break;
        case RESP_CONNECT:
            numeric = '1';
            break;
        case RESP_NO_CARRIER:
            numeric = '3';
            break;
        case RESP_ERROR:
            numeric = '4';
            break;
        case RESP_NO_DIALTONE:
            numeric = '6';
            break;
        case RESP_NO_ANSWER:
            numeric = '8';
            break;
        default:
            numeric = '4';
            break;
        }
        if (!rx_ring_push(sl, (uint8_t)numeric)) {
            sl->overrun = true;
            return;
        }
    }
    if (!rx_ring_push(sl, (uint8_t)'\r')) {
        sl->overrun = true;
    }
}

static void hangup_to_command(c64_swiftlink *sl, bool emit_no_carrier) {
    if (sl->mode == C64_SWIFTLINK_MODE_DIALING || sl->mode == C64_SWIFTLINK_MODE_ONLINE) {
        latch_hangup_req(sl);
    }
    sl->carrier_present = false;
    sl->mode = C64_SWIFTLINK_MODE_COMMAND;
    clear_escape(sl);
    sl->at_len = 0;
    sl->at_overflow = false;
    sl->ignore_lf = false;
    if (emit_no_carrier) {
        enqueue_response(sl, RESP_NO_CARRIER);
    }
}

static char norm_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static bool starts_with_ci(const char *line, const char *prefix) {
    size_t i;
    for (i = 0; prefix[i] != '\0'; ++i) {
        if (line[i] == '\0' || norm_upper(line[i]) != prefix[i]) {
            return false;
        }
    }
    return true;
}

static const char *skip_spaces(const char *p) {
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    return p;
}

static bool parse_bool01(const char *p, bool *out) {
    p = skip_spaces(p);
    if (p[0] == '=' ) {
        p = skip_spaces(p + 1);
    }
    if (p[0] == '0' && p[1] == '\0') {
        *out = false;
        return true;
    }
    if (p[0] == '1' && p[1] == '\0') {
        *out = true;
        return true;
    }
    return false;
}

static bool parse_atdt(const char *arg, char *host_out, size_t host_cap, uint16_t *port_out) {
    const char *colon;
    size_t host_len;
    unsigned long port;
    char *end = NULL;

    if (arg == NULL || arg[0] == '\0' || host_cap == 0) {
        return false;
    }
    /* No embedded spaces in host/port. */
    if (strchr(arg, ' ') != NULL || strchr(arg, '\t') != NULL) {
        return false;
    }

    colon = strrchr(arg, ':');
    if (colon != NULL && colon != arg) {
        host_len = (size_t)(colon - arg);
        if (host_len == 0 || host_len >= host_cap) {
            return false;
        }
        memcpy(host_out, arg, host_len);
        host_out[host_len] = '\0';
        if (colon[1] == '\0') {
            return false;
        }
        port = strtoul(colon + 1, &end, 10);
        if (end == colon + 1 || *end != '\0' || port == 0ul || port > 65535ul) {
            return false;
        }
        *port_out = (uint16_t)port;
        return true;
    }

    host_len = strlen(arg);
    if (host_len == 0 || host_len >= host_cap) {
        return false;
    }
    memcpy(host_out, arg, host_len + 1u);
    *port_out = 23;
    return true;
}

static void handle_at_line(c64_swiftlink *sl) {
    char line[C64_SWIFTLINK_AT_LINE_MAX + 1];
    const char *p;

    if (sl->at_overflow) {
        sl->at_len = 0;
        sl->at_overflow = false;
        enqueue_response(sl, RESP_ERROR);
        return;
    }

    memcpy(line, sl->at_line, sl->at_len);
    line[sl->at_len] = '\0';
    sl->at_len = 0;

    p = skip_spaces(line);
    if (*p == '\0') {
        return;
    }

    /* Normalize command prefix checks with uppercase compares via starts_with_ci. */
    if (!starts_with_ci(p, "AT")) {
        enqueue_response(sl, RESP_ERROR);
        return;
    }
    p += 2;
    p = skip_spaces(p);

    if (*p == '\0') {
        enqueue_response(sl, RESP_OK);
        return;
    }

    /* While dialing, further AT lines are ERROR until settle — except ATH/ATZ. */
    if (sl->mode == C64_SWIFTLINK_MODE_DIALING) {
        if (starts_with_ci(p, "H") && (p[1] == '\0' || (p[1] == '0' && p[2] == '\0'))) {
            hangup_to_command(sl, true);
            return;
        }
        if (starts_with_ci(p, "Z") && p[1] == '\0') {
            hangup_to_command(sl, true);
            modem_defaults(sl);
            return;
        }
        enqueue_response(sl, RESP_ERROR);
        return;
    }

    if (starts_with_ci(p, "Z") && p[1] == '\0') {
        if (sl->mode == C64_SWIFTLINK_MODE_ONLINE) {
            hangup_to_command(sl, true);
            modem_defaults(sl);
            return;
        }
        modem_defaults(sl);
        enqueue_response(sl, RESP_OK);
        return;
    }

    if (starts_with_ci(p, "H") && (p[1] == '\0' || (p[1] == '0' && p[2] == '\0'))) {
        if (sl->mode == C64_SWIFTLINK_MODE_ONLINE) {
            hangup_to_command(sl, true);
            return;
        }
        enqueue_response(sl, RESP_OK);
        return;
    }

    if (starts_with_ci(p, "E")) {
        bool on;
        if (!parse_bool01(p + 1, &on)) {
            enqueue_response(sl, RESP_ERROR);
            return;
        }
        sl->echo = on;
        enqueue_response(sl, RESP_OK);
        return;
    }

    if (starts_with_ci(p, "V")) {
        bool on;
        if (!parse_bool01(p + 1, &on)) {
            enqueue_response(sl, RESP_ERROR);
            return;
        }
        sl->verbose = on;
        enqueue_response(sl, RESP_OK);
        return;
    }

    if (starts_with_ci(p, "DT")) {
        char host[C64_SWIFTLINK_HOST_MAX];
        uint16_t port = 23;
        const char *arg = p + 2;

        if (sl->mode != C64_SWIFTLINK_MODE_COMMAND) {
            enqueue_response(sl, RESP_ERROR);
            return;
        }
        if (!parse_atdt(arg, host, sizeof(host), &port)) {
            enqueue_response(sl, RESP_ERROR);
            return;
        }
        latch_connect_req(sl, host, port);
        sl->mode = C64_SWIFTLINK_MODE_DIALING;
        return;
    }

    enqueue_response(sl, RESP_ERROR);
}

static bool try_echo(c64_swiftlink *sl, uint8_t byte) {
    if (!sl->echo) {
        return true;
    }
    return rx_ring_push(sl, byte);
}

static bool consume_command_byte(c64_swiftlink *sl, uint8_t byte) {
    if (byte == '\n' && sl->ignore_lf) {
        sl->ignore_lf = false;
        return true;
    }

    if (byte == '\r' || byte == '\n') {
        if (byte == '\r') {
            sl->ignore_lf = true;
        } else {
            sl->ignore_lf = false;
        }
        handle_at_line(sl);
        return true;
    }

    sl->ignore_lf = false;
    if (sl->at_overflow) {
        return true; /* keep discarding until delimiter */
    }
    if (sl->at_len >= C64_SWIFTLINK_AT_LINE_MAX) {
        sl->at_overflow = true;
        return true;
    }
    sl->at_line[sl->at_len++] = (char)byte;
    return true;
}

static bool online_flush_progress(c64_swiftlink *sl) {
    while (sl->escape_flush_pos < sl->escape_len) {
        if (!tx_ring_push(sl, (uint8_t)'+')) {
            return false;
        }
        sl->escape_flush_pos++;
    }
    if (sl->escape_flush_pos == sl->escape_len) {
        if (!tx_ring_push(sl, sl->escape_abort_byte)) {
            return false;
        }
        sl->escape_flush_pos = (uint8_t)(sl->escape_len + 1u);
    }
    sl->escape_flushing = false;
    sl->escape_len = 0;
    sl->escape_flush_pos = 0;
    sl->escape_after_guard = false;
    note_tx_activity(sl);
    return true;
}

static bool begin_escape_flush(c64_swiftlink *sl, uint8_t abort_byte) {
    sl->escape_flushing = true;
    sl->escape_flush_pos = 0;
    sl->escape_abort_byte = abort_byte;
    sl->escape_after_guard = false;
    return online_flush_progress(sl);
}

static bool consume_online_byte(c64_swiftlink *sl, uint8_t byte) {
    if (sl->escape_flushing) {
        if (!online_flush_progress(sl)) {
            return false;
        }
        return true;
    }

    /* After-guard: any TX aborts the escape and sends +++ plus this byte. */
    if (sl->escape_after_guard) {
        sl->escape_len = 3;
        return begin_escape_flush(sl, byte);
    }

    if (byte == (uint8_t)'+') {
        if (sl->escape_len == 0) {
            if (!quiet_before_ok(sl)) {
                if (!tx_ring_push(sl, byte)) {
                    return false;
                }
                note_tx_activity(sl);
                return true;
            }
            sl->escape_len = 1;
            sl->last_plus_cycle = sl->time_cycle;
            return true;
        }
        if (sl->escape_len < 2) {
            sl->escape_len++;
            sl->last_plus_cycle = sl->time_cycle;
            return true;
        }
        /* Third '+': start post-guard; never send to TCP yet. */
        sl->escape_len = 0;
        sl->escape_after_guard = true;
        sl->after_guard_start = sl->time_cycle;
        return true;
    }

    if (sl->escape_len > 0) {
        return begin_escape_flush(sl, byte);
    }

    if (!tx_ring_push(sl, byte)) {
        return false;
    }
    note_tx_activity(sl);
    return true;
}

static void service_escape_timers(c64_swiftlink *sl) {
    if (sl->mode != C64_SWIFTLINK_MODE_ONLINE) {
        return;
    }

    /* Stale partial +++ (gap > guard between pluses): flush as data. */
    if (!sl->escape_after_guard && sl->escape_len > 0 && !sl->escape_flushing) {
        if ((sl->time_cycle - sl->last_plus_cycle) >= guard_cycles(sl)) {
            sl->escape_flushing = true;
            sl->escape_flush_pos = 0;
            sl->escape_abort_byte = 0; /* no abort byte; only flush pluses */
            while (sl->escape_flush_pos < sl->escape_len) {
                if (!tx_ring_push(sl, (uint8_t)'+')) {
                    return;
                }
                sl->escape_flush_pos++;
            }
            clear_escape(sl);
            note_tx_activity(sl);
        }
    }

    if (sl->escape_after_guard &&
        (sl->time_cycle - sl->after_guard_start) >= guard_cycles(sl)) {
        hangup_to_command(sl, true);
    }
}

void c64_swiftlink_init(c64_swiftlink *sl) {
    assert(sl);
    memset(sl, 0, sizeof(*sl));
    sl->enabled = false;
    sl->base = C64_SWIFTLINK_BASE_DE00;
    sl->irq_mode = C64_SWIFTLINK_IRQ_NONE;
    sl->pace_baud = false;
    sl->time_hz = 985248u;
    cold_acia(sl);
    sl->pending_req.kind = C64_SWIFTLINK_HOST_REQ_NONE;
}

void c64_swiftlink_reset(c64_swiftlink *sl) {
    assert(sl);
    /* Silent hangup: latch host hangup if a session was active; no AT response. */
    if (sl->mode == C64_SWIFTLINK_MODE_DIALING || sl->mode == C64_SWIFTLINK_MODE_ONLINE) {
        latch_hangup_req(sl);
    } else {
        sl->pending_req.kind = C64_SWIFTLINK_HOST_REQ_NONE;
        memset(sl->pending_req.host, 0, sizeof(sl->pending_req.host));
        sl->pending_req.port = 0;
    }
    cold_acia(sl);
}

void c64_swiftlink_drop_host_session(c64_swiftlink *sl) {
    assert(sl);
    clear_rings(sl);
    sl->tx_holding = 0;
    sl->tx_holding_full = false;
    sl->rx_holding = 0;
    sl->rx_holding_full = false;
    sl->carrier_present = false;
    sl->overrun = false;
    sl->irq_latched = false;
    sl->mode = C64_SWIFTLINK_MODE_COMMAND;
    sl->at_len = 0;
    sl->ignore_lf = false;
    sl->at_overflow = false;
    clear_escape(sl);
    sl->pending_req.kind = C64_SWIFTLINK_HOST_REQ_NONE;
    memset(sl->pending_req.host, 0, sizeof(sl->pending_req.host));
    sl->pending_req.port = 0;
}

void c64_swiftlink_set_enabled(c64_swiftlink *sl, bool on) {
    assert(sl);
    sl->enabled = on;
}

void c64_swiftlink_set_base(c64_swiftlink *sl, uint16_t base) {
    assert(sl);
    sl->base = normalize_base(base);
}

void c64_swiftlink_set_irq_mode(c64_swiftlink *sl, c64_swiftlink_irq_mode mode) {
    assert(sl);
    sl->irq_mode = mode;
}

void c64_swiftlink_set_pace_baud(c64_swiftlink *sl, bool on) {
    assert(sl);
    sl->pace_baud = on;
    if (on) {
        /* Next byte is eligible immediately (unsigned wrap is fine). */
        uint64_t cpb = cycles_per_byte(sl);
        sl->last_tx_bit_cycle = sl->time_cycle - cpb;
        sl->last_rx_bit_cycle = sl->last_tx_bit_cycle;
    }
}

void c64_swiftlink_set_time(c64_swiftlink *sl, uint64_t cycle, uint32_t hz) {
    assert(sl);
    sl->time_cycle = cycle;
    if (hz != 0u) {
        sl->time_hz = hz;
    }
}

bool c64_swiftlink_irq_line(const c64_swiftlink *sl) {
    assert(sl);
    if (!sl->enabled || sl->irq_mode == C64_SWIFTLINK_IRQ_NONE) {
        return false;
    }
    return sl->irq_latched;
}

bool c64_swiftlink_owns(const c64_swiftlink *sl, uint16_t addr) {
    assert(sl);
    if (!sl->enabled) {
        return false;
    }
    return (uint16_t)(addr & 0xFF00u) == (uint16_t)(sl->base & 0xFF00u);
}

uint8_t c64_swiftlink_read(c64_swiftlink *sl, uint16_t addr) {
    uint8_t offset;

    assert(sl);

    offset = (uint8_t)(addr & 0xFFu);
    /* ASAP holding: drain TX holding / refill RX holding on every guest poll
       of data/status. Without this, service only runs at runtime batch
       boundaries (~1 byte per 1024 Phi2) and feels far slower than real
       hardware. Command/control/turbo reads do not need it. */
    if (offset == 0x00u || offset == 0x01u) {
        c64_swiftlink_service(sl);
    }

    switch (offset) {
    case 0x00:
        if (sl->rx_holding_full) {
            sl->rx_holding_full = false;
            return sl->rx_holding;
        }
        return 0x00;
    case 0x01: {
        uint8_t status = status_byte(sl);
        sl->overrun = false;
        sl->irq_latched = false;
        return status;
    }
    case 0x02:
        return sl->command;
    case 0x03:
        return sl->control;
    case 0x07:
        return turbo232_read(sl);
    default:
        return 0xFFu;
    }
}

void c64_swiftlink_write(c64_swiftlink *sl, uint16_t addr, uint8_t val) {
    uint8_t offset;

    assert(sl);

    offset = (uint8_t)(addr & 0xFFu);
    /* Drain any prior TX byte before accepting a new data write, so TDRE
       observed by a tight poll/write loop matches ASAP holding delivery. */
    if (offset == 0x00u) {
        c64_swiftlink_service(sl);
    }

    switch (offset) {
    case 0x00:
        if (sl->tx_holding_full) {
            return;
        }
        sl->tx_holding = val;
        sl->tx_holding_full = true;
        /* Try to absorb into Hayes / TX ring immediately. */
        c64_swiftlink_service(sl);
        return;
    case 0x01:
        c64_swiftlink_reset(sl);
        return;
    case 0x02:
        sl->command = val;
        update_irq_latch(sl);
        return;
    case 0x03:
        sl->control = val;
        return;
    case 0x07:
        sl->turbo232 = (uint8_t)(val & 0x03u);
        return;
    default:
        return;
    }
}

void c64_swiftlink_service(c64_swiftlink *sl) {
    uint8_t byte;

    assert(sl);

    service_escape_timers(sl);

    if (sl->escape_flushing) {
        (void)online_flush_progress(sl);
    }

    if (sl->tx_holding_full && tx_pace_ready(sl)) {
        byte = sl->tx_holding;
        if (sl->mode == C64_SWIFTLINK_MODE_ONLINE) {
            if (consume_online_byte(sl, byte)) {
                sl->tx_holding_full = false;
                sl->last_tx_bit_cycle = sl->time_cycle;
            }
        } else {
            /* Command / dialing: Hayes path (with optional echo). */
            if (try_echo(sl, byte) && consume_command_byte(sl, byte)) {
                sl->tx_holding_full = false;
                sl->last_tx_bit_cycle = sl->time_cycle;
                note_tx_activity(sl);
            }
        }
    }

    if (!sl->rx_holding_full && rx_pace_ready(sl) && rx_ring_pop(sl, &byte)) {
        sl->rx_holding = byte;
        sl->rx_holding_full = true;
        sl->last_rx_bit_cycle = sl->time_cycle;
    }

    update_irq_latch(sl);
}

bool c64_swiftlink_take_host_request(c64_swiftlink *sl, c64_swiftlink_host_req *out) {
    assert(sl);
    assert(out);

    if (sl->pending_req.kind == C64_SWIFTLINK_HOST_REQ_NONE) {
        out->kind = C64_SWIFTLINK_HOST_REQ_NONE;
        out->host[0] = '\0';
        out->port = 0;
        return false;
    }
    *out = sl->pending_req;
    sl->pending_req.kind = C64_SWIFTLINK_HOST_REQ_NONE;
    memset(sl->pending_req.host, 0, sizeof(sl->pending_req.host));
    sl->pending_req.port = 0;
    return true;
}

void c64_swiftlink_host_connect_result(c64_swiftlink *sl, c64_swiftlink_connect_err err) {
    assert(sl);

    if (sl->mode != C64_SWIFTLINK_MODE_DIALING) {
        return;
    }

    switch (err) {
    case C64_SWIFTLINK_CONN_OK:
        sl->mode = C64_SWIFTLINK_MODE_ONLINE;
        sl->carrier_present = true;
        clear_escape(sl);
        enqueue_response(sl, RESP_CONNECT);
        break;
    case C64_SWIFTLINK_CONN_NO_DIALTONE:
        sl->mode = C64_SWIFTLINK_MODE_COMMAND;
        sl->carrier_present = false;
        enqueue_response(sl, RESP_NO_DIALTONE);
        break;
    case C64_SWIFTLINK_CONN_NO_ANSWER:
        sl->mode = C64_SWIFTLINK_MODE_COMMAND;
        sl->carrier_present = false;
        enqueue_response(sl, RESP_NO_ANSWER);
        break;
    }
}

void c64_swiftlink_host_peer_closed(c64_swiftlink *sl) {
    assert(sl);
    if (sl->mode != C64_SWIFTLINK_MODE_ONLINE) {
        return;
    }
    /* Peer close: no host HANGUP request (socket already dead); AT response yes. */
    sl->carrier_present = false;
    sl->mode = C64_SWIFTLINK_MODE_COMMAND;
    clear_escape(sl);
    sl->at_len = 0;
    sl->at_overflow = false;
    sl->ignore_lf = false;
    enqueue_response(sl, RESP_NO_CARRIER);
}

size_t c64_swiftlink_pull_tx(c64_swiftlink *sl, uint8_t *out, size_t max) {
    size_t n = 0;

    assert(sl);
    if (out == NULL && max > 0) {
        return 0;
    }

    while (n < max && sl->tx_count > 0) {
        out[n++] = sl->tx_ring[sl->tx_tail];
        sl->tx_tail = (uint16_t)((sl->tx_tail + 1u) % C64_SWIFTLINK_TX_RING_SIZE);
        sl->tx_count--;
    }
    return n;
}

size_t c64_swiftlink_push_rx(c64_swiftlink *sl, const uint8_t *in, size_t n) {
    size_t accepted = 0;
    size_t i;

    assert(sl);
    if (in == NULL && n > 0) {
        return 0;
    }

    for (i = 0; i < n; ++i) {
        if (!rx_ring_push(sl, in[i])) {
            sl->overrun = true;
            break;
        }
        accepted++;
    }
    return accepted;
}

size_t c64_swiftlink_rx_space(const c64_swiftlink *sl) {
    assert(sl);
    return (size_t)(C64_SWIFTLINK_RX_RING_SIZE - sl->rx_count);
}

void c64_swiftlink_set_carrier(c64_swiftlink *sl, bool present) {
    assert(sl);
    sl->carrier_present = present;
}
