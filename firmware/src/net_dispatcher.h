#ifndef CELESTIAL_NET_DISPATCHER_H
#define CELESTIAL_NET_DISPATCHER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "net_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol constant aliases for unified validation */
#define NET_PROTOCOL_MAGIC          CELESTIAL_NET_MAGIC
#define NET_PROTOCOL_VERSION        CELESTIAL_NET_VERSION
#define NET_MSG_HEARTBEAT_ACK       NET_MSG_HEARTBEAT
#define NET_MSG_RESP_AUDIO_START    NET_MSG_RESP_START
#define NET_MSG_RESP_AUDIO_CHUNK    NET_MSG_RESP_CHUNK
#define NET_MSG_RESP_AUDIO_END      NET_MSG_RESP_END

typedef enum {
    NET_DISPATCH_OK                    = 0,
    NET_DISPATCH_ERR_NULL              = -1,
    NET_DISPATCH_ERR_SHORT_HEADER      = -2,
    NET_DISPATCH_ERR_BAD_MAGIC         = -3,
    NET_DISPATCH_ERR_BAD_VERSION       = -4,
    NET_DISPATCH_ERR_PAYLOAD_LEN       = -5,
    NET_DISPATCH_ERR_UNKNOWN_MSG       = -6,
    NET_DISPATCH_ERR_MALFORMED_PAYLOAD = -7
} net_dispatch_status_t;

typedef struct {
    uint8_t  msg_type;
    uint16_t min_payload;
    uint16_t max_payload;
    bool     require_even_payload;
    const char *name;
} net_msg_rule_t;

typedef void (*net_msg_handler_fn)(const net_packet_header_t *hdr,
                                   const uint8_t *payload,
                                   void *user_ctx);

typedef struct {
    uint8_t            msg_type;
    net_msg_handler_fn handler;
} net_dispatch_entry_t;

/**
 * Look up the payload boundary validation rule for a given protocol v2 opcode.
 * Returns NULL if msg_type is not a recognized Celestial UDP v2 message.
 */
const net_msg_rule_t* net_dispatch_get_rule(uint8_t msg_type);

/**
 * Validate raw UDP datagram bytes against the 14-byte Celestial UDP v2 header,
 * magic number, protocol version, and per-message payload boundary rules.
 * Copies the validated header into *out_hdr when out_hdr is non-NULL.
 */
net_dispatch_status_t net_dispatch_validate_packet(const uint8_t *raw_buf,
                                                   size_t total_len,
                                                   net_packet_header_t *out_hdr);

/**
 * Validate an already-extracted packet header and total datagram length against
 * the protocol magic, version, and per-message payload boundary rules.
 */
net_dispatch_status_t net_dispatch_validate_header(const net_packet_header_t *hdr,
                                                   size_t total_len);

/**
 * Look up a handler function in a dispatch table for the given msg_type.
 * Returns NULL if no matching handler is registered.
 */
net_msg_handler_fn net_dispatch_LookupHandler(const net_dispatch_entry_t *table,
                                              size_t table_len,
                                              uint8_t msg_type);

static inline net_msg_handler_fn net_dispatch_lookup_handler(const net_dispatch_entry_t *table,
                                                             size_t table_len,
                                                             uint8_t msg_type) {
    return net_dispatch_LookupHandler(table, table_len, msg_type);
}

/**
 * Validate a raw UDP packet and invoke its registered handler in `table`.
 */
net_dispatch_status_t net_dispatch_process_packet(const net_dispatch_entry_t *table,
                                                  size_t table_len,
                                                  const uint8_t *raw_buf,
                                                  size_t total_len,
                                                  void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_NET_DISPATCHER_H */
