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
    /* Host bridge rings are 4–8 KiB; keep the machine RX FIFO in the same
       ballpark so a burst banner (FICS login, etc.) is not overrun before the
       polled guest can drain RDRF. */
    C64_SWIFTLINK_TX_RING_SIZE = 1024,
    C64_SWIFTLINK_RX_RING_SIZE = 8192,
    C64_SWIFTLINK_AT_LINE_MAX = 128,
    C64_SWIFTLINK_HOST_MAX = 128
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

typedef enum c64_swiftlink_mode {
    C64_SWIFTLINK_MODE_COMMAND = 0,
    C64_SWIFTLINK_MODE_DIALING,
    C64_SWIFTLINK_MODE_ONLINE
} c64_swiftlink_mode;

typedef enum c64_swiftlink_host_req_kind {
    C64_SWIFTLINK_HOST_REQ_NONE = 0,
    C64_SWIFTLINK_HOST_REQ_CONNECT,
    C64_SWIFTLINK_HOST_REQ_HANGUP
} c64_swiftlink_host_req_kind;

typedef struct c64_swiftlink_host_req {
    c64_swiftlink_host_req_kind kind;
    char host[C64_SWIFTLINK_HOST_MAX];
    uint16_t port;
} c64_swiftlink_host_req;

typedef enum c64_swiftlink_connect_err {
    C64_SWIFTLINK_CONN_OK = 0,
    C64_SWIFTLINK_CONN_NO_DIALTONE,
    C64_SWIFTLINK_CONN_NO_ANSWER
} c64_swiftlink_connect_err;

#ifndef C64M_SWIFTLINK_TYPEDEF
#define C64M_SWIFTLINK_TYPEDEF
typedef struct c64_swiftlink c64_swiftlink;
#endif

struct c64_swiftlink {
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

    /* Hayes modem */
    c64_swiftlink_mode mode;
    bool echo;
    bool verbose;
    char at_line[C64_SWIFTLINK_AT_LINE_MAX];
    uint16_t at_len;
    bool ignore_lf; /* CR already ended the line; swallow following LF */
    bool at_overflow;

    uint8_t escape_len; /* 0..2 withheld '+' bytes while online */
    bool escape_flushing;
    uint8_t escape_flush_pos; /* next '+' index to flush, or 3 = abort byte */
    uint8_t escape_abort_byte;

    c64_swiftlink_host_req pending_req;
};

void c64_swiftlink_init(c64_swiftlink *sl);
void c64_swiftlink_reset(c64_swiftlink *sl); /* status-write / cold ACIA semantics */

void c64_swiftlink_set_enabled(c64_swiftlink *sl, bool on);
void c64_swiftlink_set_base(c64_swiftlink *sl, uint16_t base); /* DE00 or DF00 */

bool c64_swiftlink_owns(const c64_swiftlink *sl, uint16_t addr);
uint8_t c64_swiftlink_read(c64_swiftlink *sl, uint16_t addr);
void c64_swiftlink_write(c64_swiftlink *sl, uint16_t addr, uint8_t val);

/* Runtime thread only: advance Hayes, escape scanner, holding↔rings. */
void c64_swiftlink_service(c64_swiftlink *sl);

bool c64_swiftlink_take_host_request(c64_swiftlink *sl, c64_swiftlink_host_req *out);
void c64_swiftlink_host_connect_result(c64_swiftlink *sl, c64_swiftlink_connect_err err);
void c64_swiftlink_host_peer_closed(c64_swiftlink *sl);

size_t c64_swiftlink_pull_tx(c64_swiftlink *sl, uint8_t *out, size_t max);
size_t c64_swiftlink_push_rx(c64_swiftlink *sl, const uint8_t *in, size_t n);
/* Free bytes in the machine RX ring (not counting RX holding). */
size_t c64_swiftlink_rx_space(const c64_swiftlink *sl);
void c64_swiftlink_set_carrier(c64_swiftlink *sl, bool present);

#ifdef __cplusplus
}
#endif
