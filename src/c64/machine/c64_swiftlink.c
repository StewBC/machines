#include "c64_swiftlink.h"

#include <assert.h>
#include <string.h>

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
    clear_rings(sl);
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

    /* IRQ (bit 7) always 0 in v1 (polled). */
    if (!sl->carrier_present) {
        status |= C64_SWIFTLINK_STATUS_CD;
    }
    /* DSR ready (0) when enabled; inactive (1) when disabled. */
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
    return status;
}

static uint8_t turbo232_read(const c64_swiftlink *sl) {
    uint8_t value = (uint8_t)(sl->turbo232 & 0x03u);
    /* Mode bit (2): set when control baud nibble is 0000 (enhanced path). */
    if ((sl->control & 0x0Fu) == 0) {
        value |= 0x04u;
    }
    return value;
}

void c64_swiftlink_init(c64_swiftlink *sl) {
    assert(sl);
    memset(sl, 0, sizeof(*sl));
    sl->enabled = false;
    sl->base = C64_SWIFTLINK_BASE_DE00;
    cold_acia(sl);
}

void c64_swiftlink_reset(c64_swiftlink *sl) {
    assert(sl);
    /* Status-write / machine-reset ACIA path: cold chip, keep enable+base. */
    cold_acia(sl);
}

void c64_swiftlink_set_enabled(c64_swiftlink *sl, bool on) {
    assert(sl);
    sl->enabled = on;
}

void c64_swiftlink_set_base(c64_swiftlink *sl, uint16_t base) {
    assert(sl);
    sl->base = normalize_base(base);
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
    switch (offset) {
    case 0x00:
        /* Ignore data writes while TDRE is clear (holding still full). */
        if (sl->tx_holding_full) {
            return;
        }
        sl->tx_holding = val;
        sl->tx_holding_full = true;
        return;
    case 0x01:
        /* Any write to status resets the chip (RetroMate hangup path). */
        c64_swiftlink_reset(sl);
        return;
    case 0x02:
        sl->command = val;
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

    if (sl->tx_holding_full) {
        if (tx_ring_push(sl, sl->tx_holding)) {
            sl->tx_holding_full = false;
        }
        /* Else back-pressure: leave TDRE clear until the ring drains. */
    }

    if (!sl->rx_holding_full && rx_ring_pop(sl, &byte)) {
        sl->rx_holding = byte;
        sl->rx_holding_full = true;
    }
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

void c64_swiftlink_set_carrier(c64_swiftlink *sl, bool present) {
    assert(sl);
    sl->carrier_present = present;
}
