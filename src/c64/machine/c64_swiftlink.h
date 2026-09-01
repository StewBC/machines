#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    C64_SWIFTLINK_BASE_DE00 = 0xDE00u,
    C64_SWIFTLINK_BASE_DF00 = 0xDF00u,
    C64_SWIFTLINK_TX_RING_SIZE = 256,
    C64_SWIFTLINK_RX_RING_SIZE = 256
};

/* Status register bits (read). Carrier/DSR use SwiftLink pin-swap sense:
   bit 6 CD and bit 5 DSR are active-low (0 = asserted). */
enum {
    C64_SWIFTLINK_STATUS_PARITY = 0x01u,
    C64_SWIFTLINK_STATUS_FRAMING = 0x02u,
    C64_SWIFTLINK_STATUS_OVERRUN = 0x04u,
    C64_SWIFTLINK_STATUS_RDRF = 0x08u,
    C64_SWIFTLINK_STATUS_TDRE = 0x10u,
    C64_SWIFTLINK_STATUS_DSR = 0x20u,
    C64_SWIFTLINK_STATUS_CD = 0x40u,
    C64_SWIFTLINK_STATUS_IRQ = 0x80u
};

typedef struct c64_swiftlink {
    bool enabled;
    uint16_t base; /* 0xDE00 or 0xDF00 */

    uint8_t command;
    uint8_t control;
    uint8_t turbo232; /* bits 1-0 enhanced baud; mode bit is derived on read */

    uint8_t tx_holding;
    bool tx_holding_full; /* TDRE = !tx_holding_full */

    uint8_t rx_holding;
    bool rx_holding_full; /* RDRF */

    bool carrier_present; /* status bit 6: 0 when true */
    bool overrun;

    uint8_t tx_ring[C64_SWIFTLINK_TX_RING_SIZE];
    uint16_t tx_head;
    uint16_t tx_tail;
    uint16_t tx_count;

    uint8_t rx_ring[C64_SWIFTLINK_RX_RING_SIZE];
    uint16_t rx_head;
    uint16_t rx_tail;
    uint16_t rx_count;
} c64_swiftlink;

void c64_swiftlink_init(c64_swiftlink *sl);
void c64_swiftlink_reset(c64_swiftlink *sl); /* status-write / cold ACIA semantics */

void c64_swiftlink_set_enabled(c64_swiftlink *sl, bool on);
void c64_swiftlink_set_base(c64_swiftlink *sl, uint16_t base); /* DE00 or DF00 */

bool c64_swiftlink_owns(const c64_swiftlink *sl, uint16_t addr);
uint8_t c64_swiftlink_read(c64_swiftlink *sl, uint16_t addr);
void c64_swiftlink_write(c64_swiftlink *sl, uint16_t addr, uint8_t val);

/* Drain TX holding into the TX ring; fill RX holding from the RX ring.
   Hayes / online escape come in a later PR; PR1 only moves bytes. */
void c64_swiftlink_service(c64_swiftlink *sl);

size_t c64_swiftlink_pull_tx(c64_swiftlink *sl, uint8_t *out, size_t max);
size_t c64_swiftlink_push_rx(c64_swiftlink *sl, const uint8_t *in, size_t n);
void c64_swiftlink_set_carrier(c64_swiftlink *sl, bool present);

#ifdef __cplusplus
}
#endif
