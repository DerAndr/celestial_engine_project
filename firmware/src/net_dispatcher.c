#include "net_dispatcher.h"
#include <string.h>

static const net_msg_rule_t PROTOCOL_V2_RULES[] = {
    { NET_MSG_HEARTBEAT_ACK,    0,  NET_MAX_PAYLOAD_SIZE, false, "HEARTBEAT"      },
    { NET_MSG_QUERY_START,      2,  2,                    false, "QUERY_START"    },
    { NET_MSG_AUDIO_CHUNK,      2,  NET_MAX_PAYLOAD_SIZE, true,  "AUDIO_CHUNK"    },
    { NET_MSG_QUERY_END,        0,  0,                    false, "QUERY_END"      },
    { NET_MSG_RESP_AUDIO_START, 4,  4,                    false, "RESP_START"     },
    { NET_MSG_RESP_AUDIO_CHUNK, 2,  NET_MAX_PAYLOAD_SIZE, true,  "RESP_CHUNK"     },
    { NET_MSG_RESP_AUDIO_END,   0,  4,                    false, "RESP_END"       },
    { NET_MSG_CANCEL,           0,  4,                    false, "CANCEL"         },
    { NET_MSG_RESP_TEXT,        1,  NET_MAX_PAYLOAD_SIZE, false, "RESP_TEXT"      },
    { NET_MSG_ACK,              1,  4,                    false, "ACK"            },
    { NET_MSG_PROGRESS,         0,  NET_MAX_PAYLOAD_SIZE, false, "PROGRESS"       },
    { NET_MSG_ERROR,            0,  NET_MAX_PAYLOAD_SIZE, false, "ERROR"          },
    { NET_MSG_CATALOG_REQ,      0,  0,                    false, "CATALOG_REQ"    },
    { NET_MSG_CATALOG_RESP,     17, NET_MAX_PAYLOAD_SIZE, false, "CATALOG_RESP"   },
    { NET_MSG_APP_COMMAND,      6,  NET_MAX_PAYLOAD_SIZE, false, "APP_COMMAND"    },
    { NET_MSG_APP_TIME_SYNC,    12, NET_MAX_PAYLOAD_SIZE, false, "APP_TIME_SYNC"  },
    { NET_MSG_APP_TELEMETRY,    12, NET_MAX_PAYLOAD_SIZE, false, "APP_TELEMETRY"  }
};

#define PROTOCOL_V2_RULE_COUNT (sizeof(PROTOCOL_V2_RULES) / sizeof(PROTOCOL_V2_RULES[0]))

const net_msg_rule_t* net_dispatch_get_rule(uint8_t msg_type) {
    for (size_t i = 0; i < PROTOCOL_V2_RULE_COUNT; i++) {
        if (PROTOCOL_V2_RULES[i].msg_type == msg_type) {
            return &PROTOCOL_V2_RULES[i];
        }
    }
    return NULL;
}

net_dispatch_status_t net_dispatch_validate_header(const net_packet_header_t *hdr,
                                                   size_t total_len) {
    if (hdr == NULL) {
        return NET_DISPATCH_ERR_NULL;
    }
    if (total_len < sizeof(net_packet_header_t)) {
        return NET_DISPATCH_ERR_SHORT_HEADER;
    }
    if (hdr->magic != NET_PROTOCOL_MAGIC) {
        return NET_DISPATCH_ERR_BAD_MAGIC;
    }
    if (hdr->version != NET_PROTOCOL_VERSION) {
        return NET_DISPATCH_ERR_BAD_VERSION;
    }
    if (hdr->payload_len > NET_MAX_PAYLOAD_SIZE ||
        total_len != sizeof(net_packet_header_t) + (size_t)hdr->payload_len) {
        return NET_DISPATCH_ERR_PAYLOAD_LEN;
    }

    const net_msg_rule_t *rule = net_dispatch_get_rule(hdr->msg_type);
    if (rule == NULL) {
        return NET_DISPATCH_ERR_UNKNOWN_MSG;
    }
    if (hdr->payload_len < rule->min_payload || hdr->payload_len > rule->max_payload) {
        return NET_DISPATCH_ERR_MALFORMED_PAYLOAD;
    }
    if (rule->require_even_payload && (hdr->payload_len & 1u) != 0u) {
        return NET_DISPATCH_ERR_MALFORMED_PAYLOAD;
    }
    return NET_DISPATCH_OK;
}

net_dispatch_status_t net_dispatch_validate_packet(const uint8_t *raw_buf,
                                                   size_t total_len,
                                                   net_packet_header_t *out_hdr) {
    if (raw_buf == NULL) {
        return NET_DISPATCH_ERR_NULL;
    }
    if (total_len < sizeof(net_packet_header_t)) {
        return NET_DISPATCH_ERR_SHORT_HEADER;
    }

    net_packet_header_t hdr;
    memcpy(&hdr, raw_buf, sizeof(net_packet_header_t));

    net_dispatch_status_t st = net_dispatch_validate_header(&hdr, total_len);
    if (st != NET_DISPATCH_OK) {
        return st;
    }
    if (out_hdr != NULL) {
        *out_hdr = hdr;
    }
    return NET_DISPATCH_OK;
}

net_msg_handler_fn net_dispatch_LookupHandler(const net_dispatch_entry_t *table,
                                              size_t table_len,
                                              uint8_t msg_type) {
    if (table == NULL || table_len == 0) {
        return NULL;
    }
    for (size_t i = 0; i < table_len; i++) {
        if (table[i].msg_type == msg_type) {
            return table[i].handler;
        }
    }
    return NULL;
}

net_dispatch_status_t net_dispatch_process_packet(const net_dispatch_entry_t *table,
                                                  size_t table_len,
                                                  const uint8_t *raw_buf,
                                                  size_t total_len,
                                                  void *user_ctx) {
    net_packet_header_t hdr;
    net_dispatch_status_t st = net_dispatch_validate_packet(raw_buf, total_len, &hdr);
    if (st != NET_DISPATCH_OK) {
        return st;
    }

    net_msg_handler_fn fn = net_dispatch_LookupHandler(table, table_len, hdr.msg_type);
    if (fn == NULL) {
        return NET_DISPATCH_ERR_UNKNOWN_MSG;
    }

    const uint8_t *payload = (hdr.payload_len > 0) ? (raw_buf + sizeof(net_packet_header_t)) : NULL;
    fn(&hdr, payload, user_ctx);
    return NET_DISPATCH_OK;
}
