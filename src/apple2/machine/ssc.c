#include "ssc.h"

#include <stddef.h>

/*
 * Super Serial Card 6551 ACIA + DIP reads.
 * TX routes through sink seam (v1: ImageWriter when attached).
 */

static bool tx_irq_enabled(const apple2_ssc *s)
{
    /* DTR (bit0) and transmitter IRQ select bits 3-2 == 01. */
    return (s->command & 0x01u) != 0u && ((s->command >> 2) & 0x03u) == 0x01u;
}

static bool rx_irq_enabled(const apple2_ssc *s)
{
    /* DTR on and receiver IRQ not disabled (bit1 == 0). */
    return (s->command & 0x01u) != 0u && (s->command & 0x02u) == 0u;
}

static void update_irq_latch(apple2_ssc *s)
{
    bool tdre = !s->tx_holding_full;
    bool rdrf = s->rx_holding_full;

    if (tx_irq_enabled(s) && tdre && !s->prev_tdre) {
        s->irq_latched = true;
    }
    if (rx_irq_enabled(s) && rdrf && !s->prev_rdrf) {
        s->irq_latched = true;
    }
    s->prev_tdre = tdre;
    s->prev_rdrf = rdrf;
}

static void absorb_tx(apple2_ssc *s)
{
    if (!s->tx_holding_full) {
        return;
    }
    s->last_tx = s->tx_holding;
    if (s->sink == A2_SSC_SINK_IMAGEWRITER && s->sink_putc != NULL) {
        s->sink_putc(s->sink_user, s->tx_holding);
    }
    s->tx_holding_full = false;
    update_irq_latch(s);
}

static uint8_t status_byte(const apple2_ssc *s)
{
    uint8_t status = 0;

    /* Printer bundle: DCD/DSR ready ⇒ bits clear (active-low sense). */
    if (!s->tx_holding_full) {
        status |= SSC_STATUS_TDRE;
    }
    if (s->rx_holding_full) {
        status |= SSC_STATUS_RDRF;
    }
    if (s->irq_latched) {
        status |= SSC_STATUS_IRQ;
    }
    return status;
}

void ssc_reset(apple2_ssc *s)
{
    if (s == NULL) {
        return;
    }
    s->command = 0;
    s->control = 0;
    s->tx_holding = 0;
    s->tx_holding_full = false;
    s->rx_holding = 0;
    s->rx_holding_full = false;
    s->irq_latched = false;
    s->prev_tdre = true; /* TDRE idle after reset */
    s->prev_rdrf = false;
    s->last_tx = 0;
    /* Keep sink kind / putc across programmed ACIA reset; attach wires them. */
}

uint8_t ssc_read_c0n(apple2_ssc *s, uint8_t offset)
{
    uint8_t reg;

    if (s == NULL) {
        return 0;
    }

    /* DIPs at offsets 1 and 2 (and mirrors where bit3 clear in some maps). */
    if ((offset & 0x08u) == 0u) {
        if (offset == 1u) {
            return (uint8_t)SSC_DIP1_PRINTER;
        }
        if (offset == 2u) {
            return (uint8_t)SSC_DIP2_PRINTER;
        }
        return 0;
    }

    reg = (uint8_t)(offset & 0x03u);
    if (reg == 0u || reg == 1u) {
        absorb_tx(s);
    }

    switch (reg) {
    case 0: /* RDR — no RX for printer-only */
        if (s->rx_holding_full) {
            s->rx_holding_full = false;
            update_irq_latch(s);
            return s->rx_holding;
        }
        return 0;
    case 1: { /* Status — reading clears IRQ */
        uint8_t status = status_byte(s);
        s->irq_latched = false;
        return status;
    }
    case 2:
        return s->command;
    case 3:
        return s->control;
    default:
        return 0;
    }
}

void ssc_write_c0n(apple2_ssc *s, uint8_t offset, uint8_t value)
{
    uint8_t reg;

    if (s == NULL) {
        return;
    }
    if ((offset & 0x08u) == 0u) {
        return; /* DIP / open bus — writes ignored */
    }

    reg = (uint8_t)(offset & 0x03u);
    switch (reg) {
    case 0: /* TDR */
        if (s->tx_holding_full) {
            return;
        }
        s->tx_holding = value;
        s->tx_holding_full = true;
        update_irq_latch(s);
        /* Instant/near-instant TX for v1 (baud does not gate raster accept). */
        absorb_tx(s);
        return;
    case 1: /* Programmed reset */
        ssc_reset(s);
        return;
    case 2: {
        bool was_tx_irq = tx_irq_enabled(s);
        s->command = value;
        /*
         * Enabling TX IRQ while TDRE is already set must assert IRQ
         * (6551 level condition; not only rising TDRE edges).
         */
        if (tx_irq_enabled(s) && !s->tx_holding_full &&
            (!was_tx_irq || !s->prev_tdre)) {
            s->irq_latched = true;
        }
        update_irq_latch(s);
        return;
    }
    case 3:
        /* Baud/word/stop stored; external clock bit ignored (force internal). */
        s->control = (uint8_t)(value | 0x10u);
        return;
    default:
        return;
    }
}

uint8_t ssc_irq_pending(const apple2_ssc *s)
{
    if (s == NULL || !s->irq_latched) {
        return 0;
    }
    /* IRQ delivery requires DTR (bit0); matches 6551 / Command enable path. */
    if ((s->command & 0x01u) == 0u) {
        return 0;
    }
    return 1;
}
