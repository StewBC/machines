#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sink seam for SSC TX. PR 3 discards bytes (NONE). PR 5 wires IMAGEWRITER.
 * HOST_SERIAL reserved for later PTY/TCP/modem.
 */
typedef enum a2_ssc_sink_kind {
    A2_SSC_SINK_IMAGEWRITER = 0,
    A2_SSC_SINK_NONE,
    A2_SSC_SINK_HOST_SERIAL
} a2_ssc_sink_kind;

/* Classic MAME a2ssc printer-bundle DIP reads (design normative). */
enum {
    SSC_DIP1_PRINTER = 0xE8u, /* $C0n1: 9600 + Printer Mode + normal CTS */
    SSC_DIP2_PRINTER = 0x00u  /* $C0n2: 8N1, LF-after-CR on, IRQ path default */
};

/* MOS6551 status bits (active-low DCD/DSR: 0 = ready/asserted). */
enum {
    SSC_STATUS_PARITY = 0x01u,
    SSC_STATUS_FRAMING = 0x02u,
    SSC_STATUS_OVERRUN = 0x04u,
    SSC_STATUS_RDRF = 0x08u,
    SSC_STATUS_TDRE = 0x10u,
    SSC_STATUS_DCD = 0x20u,
    SSC_STATUS_DSR = 0x40u,
    SSC_STATUS_IRQ = 0x80u
};

typedef struct apple2_ssc {
    uint8_t command;
    uint8_t control;

    uint8_t tx_holding;
    bool tx_holding_full; /* TDRE = !tx_holding_full */

    uint8_t rx_holding;
    bool rx_holding_full; /* RDRF; unused for printer-only */

    bool irq_latched;
    bool prev_tdre;
    bool prev_rdrf;

    a2_ssc_sink_kind sink;
    uint8_t last_tx; /* most recent discarded TX byte (tests / debug) */
} apple2_ssc;

void ssc_reset(apple2_ssc *s);

uint8_t ssc_read_c0n(apple2_ssc *s, uint8_t offset);
void ssc_write_c0n(apple2_ssc *s, uint8_t offset, uint8_t value);

/* Non-zero when 6551 IRQ is latched and Command enables IRQs (DTR path). */
uint8_t ssc_irq_pending(const apple2_ssc *s);

#ifdef __cplusplus
}
#endif
