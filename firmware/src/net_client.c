#include "net_client.h"
#include "net_protocol.h"
#include "net_dispatcher.h"
#include "audio.h"
#include "response_gate.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include <string.h>
#include <stdio.h>

#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#define HAVE_WIFI_CREDENTIALS 1
#else
#define HAVE_WIFI_CREDENTIALS 0
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define BRAIN_SERVER_IP "192.168.178.100"
#define BRAIN_SERVER_PORT 5005
#endif
#ifndef BRAIN_SERVER_IP
#define BRAIN_SERVER_IP "192.168.178.100"
#endif
#ifndef BRAIN_SERVER_PORT
#define BRAIN_SERVER_PORT CELESTIAL_NET_DEFAULT_PORT
#endif
#ifndef WIFI_FALLBACK_SSID
#define WIFI_FALLBACK_SSID ""
#endif
#ifndef WIFI_FALLBACK_PASSWORD
#define WIFI_FALLBACK_PASSWORD ""
#endif
#ifndef BRAIN_FALLBACK_SERVER_IP
#define BRAIN_FALLBACK_SERVER_IP BRAIN_SERVER_IP
#endif
#ifndef BRAIN_FALLBACK_SERVER_PORT
#define BRAIN_FALLBACK_SERVER_PORT BRAIN_SERVER_PORT
#endif
#ifndef CELESTIAL_AUTH_SECRET
#define CELESTIAL_AUTH_SECRET ""
#endif

static inline uint32_t compute_auth_token(const char *key) {
    uint32_t hash = 0x811c9dc5u;
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 0x01000193u;
    }
    return hash;
}

static ip_addr_t companion_ip_addr;
static u16_t companion_port = CELESTIAL_NET_DEFAULT_PORT;
static bool has_paired_companion = false;

#define REMOTE_CMD_QUEUE_LEN 16
static remote_cmd_t remote_cmd_queue[REMOTE_CMD_QUEUE_LEN];
static volatile uint8_t remote_cmd_head = 0;
static volatile uint8_t remote_cmd_tail = 0;

static inline void enqueue_remote_command(const remote_cmd_t *cmd) {
    uint8_t next_head = (remote_cmd_head + 1) % REMOTE_CMD_QUEUE_LEN;
    if (next_head != remote_cmd_tail) {
        remote_cmd_queue[remote_cmd_head] = *cmd;
        remote_cmd_head = next_head;
    }
}

static ip_addr_t pi5_server_ip_addr;
static uint16_t pi5_server_port = BRAIN_SERVER_PORT;
static uint32_t last_pi5_heartbeat_rx_ms = 0;
static uint32_t last_phone_heartbeat_rx_ms = 0;
static uint32_t last_catalog_req_ms = 0;

static net_host_type_t active_host_type = HOST_TYPE_PHONE;
static net_host_type_t preferred_host = HOST_TYPE_PHONE;
static volatile bool net_cancel_flag = false;
static char active_host_name[16] = "PHONE BRAIN";

static const net_catalog_resp_t DEFAULT_PI5_CATALOG = {
    .host_type = HOST_TYPE_PI5,
    .host_name = "PI 5 BRAIN",
    .model_count = 8,
    .models = {
        { 0, 0, "DEEP THOUGHT",   "42"  },
        { 1, 1, "EDDIE (8B)",     "EDD" },
        { 2, 2, "GROK 4.5",       "GRK" },
        { 3, 3, "MARVIN (70B)",   "MRV" },
        { 4, 4, "QWEN 0.5B FAST", "Q05" },
        { 5, 0, "GEMMA 4 (E2B)",  "G4"  },
        { 6, 2, "DEEPSEEK R1",    "R1"  },
        { 7, 3, "QWEN 1.5B CORE", "Q15" }
    }
};

static const net_catalog_resp_t DEFAULT_PHONE_CATALOG = {
    .host_type = HOST_TYPE_PHONE,
    .host_name = "PHONE BRAIN",
    .model_count = 4,
    .models = {
        { 0, 4, "Gemma 4 (E2B)",  "G4-MM" },
        { 1, 1, "Gemma 4 Think",  "G4THK" },
        { 2, 0, "Gemini Nano",    "NANO"  },
        { 3, 2, "Pi 5 Core",      "PI5"   }
    }
};

static net_catalog_resp_t active_catalog;
static net_catalog_resp_t pi5_catalog_cache;
static bool has_pi5_catalog = false;
static net_catalog_resp_t phone_catalog_cache;
static bool has_phone_catalog = false;

static int active_ap_profile = 0; /* 0 = Primary, 1 = Fallback Hotspot */
static int ap_connect_failures = 0;
static uint16_t active_server_port = BRAIN_SERVER_PORT;

static const char* get_current_ssid(void) {
    if (active_ap_profile == 1 && strlen(WIFI_FALLBACK_SSID) > 0) {
        return WIFI_FALLBACK_SSID;
    }
    return WIFI_SSID;
}

static const char* get_current_password(void) {
    if (active_ap_profile == 1 && strlen(WIFI_FALLBACK_SSID) > 0) {
        return WIFI_FALLBACK_PASSWORD;
    }
    return WIFI_PASSWORD;
}

static const char* get_current_server_ip_str(void) {
    if (active_ap_profile == 1 && strlen(WIFI_FALLBACK_SSID) > 0 && strlen(BRAIN_FALLBACK_SERVER_IP) > 0) {
        return BRAIN_FALLBACK_SERVER_IP;
    }
    return BRAIN_SERVER_IP;
}

static uint16_t get_current_server_port(void) {
    if (active_ap_profile == 1 && strlen(WIFI_FALLBACK_SSID) > 0) {
        return BRAIN_FALLBACK_SERVER_PORT;
    }
    return BRAIN_SERVER_PORT;
}

static net_status_t current_status = NET_STATUS_DISCONNECTED;
static char current_ip_str[24] = "0.0.0.0";
static struct udp_pcb *udp_client_pcb = NULL;
static ip_addr_t server_ip_addr;
static uint16_t tx_seq_counter = 0;
static uint32_t last_status_check_ms = 0;

// Response buffer & streaming state
static volatile bool resp_is_receiving = false;
static volatile bool resp_is_ready = false;
static char resp_text_buffer[512] = "";
static volatile bool unsolicited_text_ready = false;

static uint32_t last_server_heartbeat_rx_ms = 0;
static uint32_t last_heartbeat_tx_ms = 0;

static uint32_t last_resp_chunk_rx_ms = 0;
static response_gate_t response_gate;
static uint32_t request_counter;
static volatile bool response_failed;
static volatile uint32_t response_progress_ms;
static uint32_t response_expected_samples, response_received_samples;

/* Called only inside the lwIP callback, which already owns the SDK lock. */
static void acknowledge_response(const net_packet_header_t *received, const ip_addr_t *dst_ip, uint16_t dst_port) {
    struct pbuf *ack = pbuf_alloc(PBUF_TRANSPORT, sizeof(*received) + 1, PBUF_RAM);
    if (!ack) return;
    net_packet_header_t hdr = { CELESTIAL_NET_MAGIC, CELESTIAL_NET_VERSION,
        NET_MSG_ACK, received->seq_num, 1, received->request_id };
    memcpy(ack->payload, &hdr, sizeof hdr);
    ((uint8_t *)ack->payload)[sizeof hdr] = received->msg_type;
    udp_sendto(udp_client_pcb, ack, dst_ip, dst_port);
    pbuf_free(ack);
}

static void sanitize_oled_text(char *dst, const uint8_t *src, size_t src_len, size_t max_dst) {
    if (dst == NULL || max_dst == 0) return;
    if (src == NULL || src_len == 0) {
        dst[0] = '\0';
        return;
    }
    size_t out_idx = 0;
    for (size_t i = 0; i < src_len && out_idx + 4 < max_dst; ) {
        uint8_t b1 = src[i];
        if (b1 < 128) {
            if (b1 >= 32 && b1 <= 126) {
                dst[out_idx++] = (char)b1;
            } else if (b1 == '\n' || b1 == '\r' || b1 == '\t') {
                dst[out_idx++] = ' ';
            }
            i++;
        } else if ((b1 == 0xC3) && (i + 1 < src_len)) {
            uint8_t b2 = src[i + 1];
            if (b2 == 0xA4) { dst[out_idx++] = 'a'; dst[out_idx++] = 'e'; }
            else if (b2 == 0xB6) { dst[out_idx++] = 'o'; dst[out_idx++] = 'e'; }
            else if (b2 == 0xBC) { dst[out_idx++] = 'u'; dst[out_idx++] = 'e'; }
            else if (b2 == 0x9F) { dst[out_idx++] = 's'; dst[out_idx++] = 's'; }
            else if (b2 == 0x84) { dst[out_idx++] = 'A'; dst[out_idx++] = 'e'; }
            else if (b2 == 0x96) { dst[out_idx++] = 'O'; dst[out_idx++] = 'e'; }
            else if (b2 == 0x9C) { dst[out_idx++] = 'U'; dst[out_idx++] = 'e'; }
            else { dst[out_idx++] = ' '; }
            i += 2;
        } else if ((b1 == 0xD0 || b1 == 0xD1) && (i + 1 < src_len)) {
            uint8_t b2 = src[i + 1];
            uint16_t code = ((uint16_t)b1 << 8) | b2;
            const char *tr = " ";
            switch (code) {
                case 0xD090: tr = "A"; break; case 0xD091: tr = "B"; break;
                case 0xD092: tr = "V"; break; case 0xD093: tr = "G"; break;
                case 0xD094: tr = "D"; break; case 0xD095: tr = "E"; break;
                case 0xD081: tr = "Yo"; break; case 0xD096: tr = "Zh"; break;
                case 0xD097: tr = "Z"; break; case 0xD098: tr = "I"; break;
                case 0xD099: tr = "Y"; break; case 0xD09A: tr = "K"; break;
                case 0xD09B: tr = "L"; break; case 0xD09C: tr = "M"; break;
                case 0xD09D: tr = "N"; break; case 0xD09E: tr = "O"; break;
                case 0xD09F: tr = "P"; break; case 0xD0A0: tr = "R"; break;
                case 0xD0A1: tr = "S"; break; case 0xD0A2: tr = "T"; break;
                case 0xD0A3: tr = "U"; break; case 0xD0A4: tr = "F"; break;
                case 0xD0A5: tr = "Kh"; break; case 0xD0A6: tr = "Ts"; break;
                case 0xD0A7: tr = "Ch"; break; case 0xD0A8: tr = "Sh"; break;
                case 0xD0A9: tr = "Shch"; break; case 0xD0AA: tr = ""; break;
                case 0xD0AB: tr = "Y"; break; case 0xD0AC: tr = ""; break;
                case 0xD0AD: tr = "E"; break; case 0xD0AE: tr = "Yu"; break;
                case 0xD0AF: tr = "Ya"; break;
                case 0xD0B0: tr = "a"; break; case 0xD0B1: tr = "b"; break;
                case 0xD0B2: tr = "v"; break; case 0xD0B3: tr = "g"; break;
                case 0xD0B4: tr = "d"; break; case 0xD0B5: tr = "e"; break;
                case 0xD191: tr = "yo"; break; case 0xD0B6: tr = "zh"; break;
                case 0xD0B7: tr = "z"; break; case 0xD0B8: tr = "i"; break;
                case 0xD0B9: tr = "y"; break; case 0xD0BA: tr = "k"; break;
                case 0xD0BB: tr = "l"; break; case 0xD0BC: tr = "m"; break;
                case 0xD0BD: tr = "n"; break; case 0xD0BE: tr = "o"; break;
                case 0xD0BF: tr = "p"; break; case 0xD180: tr = "r"; break;
                case 0xD181: tr = "s"; break; case 0xD182: tr = "t"; break;
                case 0xD183: tr = "u"; break; case 0xD184: tr = "f"; break;
                case 0xD185: tr = "kh"; break; case 0xD186: tr = "ts"; break;
                case 0xD187: tr = "ch"; break; case 0xD188: tr = "sh"; break;
                case 0xD189: tr = "shch"; break; case 0xD18A: tr = ""; break;
                case 0xD18B: tr = "y"; break; case 0xD18C: tr = ""; break;
                case 0xD18D: tr = "e"; break; case 0xD18E: tr = "yu"; break;
                case 0xD18F: tr = "ya"; break;
                default: tr = " "; break;
            }
            while (*tr && out_idx + 1 < max_dst) {
                dst[out_idx++] = *tr++;
            }
            i += 2;
        } else if (b1 == 0xE2 && i + 2 < src_len) {
            uint8_t b2 = src[i+1], b3 = src[i+2];
            if (b2 == 0x80 && (b3 == 0x98 || b3 == 0x99)) dst[out_idx++] = '\'';
            else if (b2 == 0x80 && (b3 == 0x9C || b3 == 0x9D)) dst[out_idx++] = '"';
            else if (b2 == 0x80 && (b3 == 0x93 || b3 == 0x94)) dst[out_idx++] = '-';
            else dst[out_idx++] = ' ';
            i += 3;
        } else {
            dst[out_idx++] = ' ';
            i++;
        }
    }
    dst[out_idx] = '\0';
}

typedef struct {
    const ip_addr_t *addr;
    u16_t port;
    uint32_t now;
} net_rx_context_t;

static inline bool is_authorized_response_source(const net_rx_context_t *ctx) {
    bool is_from_server = (ip_addr_cmp(ctx->addr, &server_ip_addr) && ctx->port == active_server_port);
    bool is_from_companion = (has_paired_companion && ip_addr_cmp(ctx->addr, &companion_ip_addr));
    return is_from_server || is_from_companion;
}

static void handle_rx_app_command(const net_packet_header_t *hdr,
                                  const uint8_t *payload,
                                  void *user_ctx) {
    (void)hdr;
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (payload == NULL) return;

    uint32_t token = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                     ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
    uint32_t expected = compute_auth_token(CELESTIAL_AUTH_SECRET);
    if (strlen(CELESTIAL_AUTH_SECRET) == 0 || token == expected) {
        companion_ip_addr = *ctx->addr;
        companion_port = CELESTIAL_NET_DEFAULT_PORT;
        has_paired_companion = true;
        remote_cmd_t cmd = {0};
        cmd.cmd_code = payload[4];
        cmd.param = payload[5];
        cmd.has_command = true;
        enqueue_remote_command(&cmd);
    }
}

static void handle_rx_app_time_sync(const net_packet_header_t *hdr,
                                    const uint8_t *payload,
                                    void *user_ctx) {
    (void)hdr;
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (payload == NULL) return;

    uint32_t token = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                     ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
    uint32_t expected = compute_auth_token(CELESTIAL_AUTH_SECRET);
    if (strlen(CELESTIAL_AUTH_SECRET) == 0 || token == expected) {
        companion_ip_addr = *ctx->addr;
        companion_port = CELESTIAL_NET_DEFAULT_PORT;
        has_paired_companion = true;
        remote_cmd_t cmd = {0};
        cmd.cmd_code = 'X';
        cmd.val32 = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) |
                    ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
        cmd.val32_extra = (uint32_t)payload[8] | ((uint32_t)payload[9] << 8) |
                          ((uint32_t)payload[10] << 16) | ((uint32_t)payload[11] << 24);
        cmd.has_command = true;
        enqueue_remote_command(&cmd);
    }
}

static void handle_rx_cancel(const net_packet_header_t *hdr,
                             const uint8_t *payload,
                             void *user_ctx) {
    (void)hdr;
    (void)payload;
    (void)user_ctx;
    /* Already inside lwIP RX callback lock — do not re-enter or call send_udp_packet here */
    response_gate_cancel(&response_gate);
    resp_is_receiving = false;
    resp_is_ready = false;
    net_cancel_flag = true;
}

static void handle_rx_catalog_resp(const net_packet_header_t *hdr,
                                   const uint8_t *payload,
                                   void *user_ctx) {
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (payload == NULL) return;

    uint8_t h_type = payload[0];
    char h_name[16];
    memcpy(h_name, &payload[1], 15);
    h_name[15] = '\0';
    uint8_t m_count = payload[16];
    if (m_count > NET_MAX_CATALOG_MODELS) m_count = NET_MAX_CATALOG_MODELS;

    net_catalog_resp_t cat;
    memset(&cat, 0, sizeof(cat));
    memcpy(cat.host_name, h_name, sizeof(cat.host_name) - 1);
    cat.host_name[sizeof(cat.host_name) - 1] = '\0';
    cat.model_count = m_count;

    size_t offset = 17;
    for (uint8_t i = 0; i < m_count && (offset + 26 <= hdr->payload_len); i++) {
        cat.models[i].model_id = payload[offset];
        cat.models[i].theme_idx = payload[offset + 1];
        memcpy(cat.models[i].name, &payload[offset + 2], 18);
        cat.models[i].name[17] = '\0';
        memcpy(cat.models[i].tag, &payload[offset + 20], 6);
        cat.models[i].tag[5] = '\0';
        offset += 26;
    }

    if (h_type == HOST_TYPE_PI5) {
        last_pi5_heartbeat_rx_ms = ctx->now;
        pi5_server_ip_addr = *ctx->addr;
        memcpy(&pi5_catalog_cache, &cat, sizeof(cat));
        has_pi5_catalog = true;
        if (preferred_host != HOST_TYPE_PHONE || (active_ap_profile == 1 && !has_paired_companion)) {
            last_server_heartbeat_rx_ms = ctx->now;
            active_host_type = HOST_TYPE_PI5;
            server_ip_addr = *ctx->addr;
            active_server_port = pi5_server_port;
            memcpy(&active_catalog, &pi5_catalog_cache, sizeof(active_catalog));
            strncpy(active_host_name, pi5_catalog_cache.host_name, sizeof(active_host_name));
        }
    } else if (h_type == HOST_TYPE_PHONE) {
        last_phone_heartbeat_rx_ms = ctx->now;
        companion_ip_addr = *ctx->addr;
        companion_port = CELESTIAL_NET_DEFAULT_PORT;
        has_paired_companion = true;
        memcpy(&phone_catalog_cache, &cat, sizeof(cat));
        has_phone_catalog = true;

        // When phone pushes its catalog, activate it and lock preferred_host to PHONE
        preferred_host = HOST_TYPE_PHONE;
        active_host_type = HOST_TYPE_PHONE;
        server_ip_addr = *ctx->addr;
        active_server_port = CELESTIAL_NET_DEFAULT_PORT;
        last_server_heartbeat_rx_ms = ctx->now;
        memcpy(&active_catalog, &phone_catalog_cache, sizeof(active_catalog));
        strncpy(active_host_name, phone_catalog_cache.host_name, sizeof(active_host_name));
    }
}

static void handle_rx_heartbeat(const net_packet_header_t *hdr,
                                const uint8_t *payload,
                                void *user_ctx) {
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (hdr->request_id != 0) return;

    // 1. Heartbeat from Pi 5 (even if server_ip_addr was temporarily pointing to Phone)
    if (ip_addr_cmp(ctx->addr, &pi5_server_ip_addr) && ctx->port == pi5_server_port) {
        last_pi5_heartbeat_rx_ms = ctx->now;
        if (preferred_host != HOST_TYPE_PHONE || (active_ap_profile == 1 && !has_paired_companion)) {
            last_server_heartbeat_rx_ms = ctx->now;
            if (active_host_type != HOST_TYPE_PI5) {
                active_host_type = HOST_TYPE_PI5;
                server_ip_addr = pi5_server_ip_addr;
                active_server_port = pi5_server_port;
                if (has_pi5_catalog) {
                    memcpy(&active_catalog, &pi5_catalog_cache, sizeof(active_catalog));
                    strncpy(active_host_name, pi5_catalog_cache.host_name, sizeof(active_host_name));
                } else {
                    memcpy(&active_catalog, &DEFAULT_PI5_CATALOG, sizeof(active_catalog));
                    strncpy(active_host_name, DEFAULT_PI5_CATALOG.host_name, sizeof(active_host_name));
                }
            }
        }
        if (payload != NULL && hdr->payload_len >= (uint16_t)sizeof(uint32_t)) {
            uint32_t server_epoch = 0;
            memcpy(&server_epoch, payload, sizeof(uint32_t));
            if (server_epoch > 1700000000u) {
                extern void set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis);
                set_current_time_epoch(server_epoch, 0);
            }
        }
        return;
    }

    // 2. Heartbeat from Phone / Companion App (not from Pi 5)
    last_phone_heartbeat_rx_ms = ctx->now;
    companion_ip_addr = *ctx->addr;
    companion_port = CELESTIAL_NET_DEFAULT_PORT;
    has_paired_companion = true;

    if (preferred_host == HOST_TYPE_PHONE ||
        (preferred_host == HOST_TYPE_NONE && (ctx->now - last_pi5_heartbeat_rx_ms > 4000u))) {
        active_host_type = HOST_TYPE_PHONE;
        server_ip_addr = companion_ip_addr;
        active_server_port = CELESTIAL_NET_DEFAULT_PORT;
        last_server_heartbeat_rx_ms = ctx->now;
        if (has_phone_catalog) {
            memcpy(&active_catalog, &phone_catalog_cache, sizeof(active_catalog));
            strncpy(active_host_name, phone_catalog_cache.host_name, sizeof(active_host_name));
        } else {
            memcpy(&active_catalog, &DEFAULT_PHONE_CATALOG, sizeof(active_catalog));
            strncpy(active_host_name, DEFAULT_PHONE_CATALOG.host_name, sizeof(active_host_name));
        }
    }
    if (payload != NULL && hdr->payload_len >= (uint16_t)sizeof(uint32_t)) {
        uint32_t server_epoch = 0;
        memcpy(&server_epoch, payload, sizeof(uint32_t));
        if (server_epoch > 1700000000u) {
            extern void set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis);
            set_current_time_epoch(server_epoch, 0);
        }
        /* Incoming packet with 4-byte epoch is already a Heartbeat ACK; do NOT echo back! */
        return;
    }

    // Only reply to a 0-byte Heartbeat ping from Companion so we never trigger an ACK ping-pong loop
    struct pbuf *reply = pbuf_alloc(PBUF_TRANSPORT, sizeof(net_packet_header_t), PBUF_RAM);
    if (reply != NULL) {
        net_packet_header_t *rhdr = (net_packet_header_t *)reply->payload;
        rhdr->magic = CELESTIAL_NET_MAGIC;
        rhdr->version = CELESTIAL_NET_VERSION;
        rhdr->msg_type = NET_MSG_HEARTBEAT;
        rhdr->seq_num = tx_seq_counter++;
        rhdr->payload_len = 0;
        rhdr->request_id = 0;
        udp_sendto(udp_client_pcb, reply, ctx->addr, ctx->port);
        pbuf_free(reply);
    }
}

static void handle_rx_resp_text(const net_packet_header_t *hdr,
                                const uint8_t *payload,
                                void *user_ctx) {
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (payload == NULL || hdr->payload_len == 0) return;

    // Direct Text Broadcast from Companion App / Brain Host when not matching an active gate
    if (!response_gate_matches(&response_gate, hdr->request_id)) {
        uint16_t copy_len = (hdr->payload_len < sizeof(resp_text_buffer))
                            ? hdr->payload_len
                            : (uint16_t)(sizeof(resp_text_buffer) - 1);
        sanitize_oled_text(resp_text_buffer, payload, copy_len, sizeof(resp_text_buffer));

        companion_ip_addr = *ctx->addr;
        companion_port = CELESTIAL_NET_DEFAULT_PORT;
        has_paired_companion = true;
        unsolicited_text_ready = true;

        acknowledge_response(hdr, ctx->addr, ctx->port);
        return;
    }

    if (!is_authorized_response_source(ctx)) return;

    uint16_t copy_len = (hdr->payload_len < sizeof(resp_text_buffer))
                        ? hdr->payload_len
                        : (uint16_t)(sizeof(resp_text_buffer) - 1);
    sanitize_oled_text(resp_text_buffer, payload, copy_len, sizeof(resp_text_buffer));
    response_progress_ms = ctx->now;
    acknowledge_response(hdr, ctx->addr, ctx->port);
}

static void handle_rx_resp_start(const net_packet_header_t *hdr,
                                 const uint8_t *payload,
                                 void *user_ctx) {
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (payload == NULL || hdr->payload_len != sizeof(uint32_t)) return;

    // Pair with companion / brain host and open gate for direct or active stream
    companion_ip_addr = *ctx->addr;
    companion_port = CELESTIAL_NET_DEFAULT_PORT;
    has_paired_companion = true;
    response_gate_begin(&response_gate, hdr->request_id);

    if (!is_authorized_response_source(ctx)) return;
    if (!response_gate_matches(&response_gate, hdr->request_id)) return;

    bool first = !response_gate.started;
    bool ack = response_gate_start(&response_gate, hdr->request_id);
    if (ack && first) {
        memcpy(&response_expected_samples, payload, sizeof(uint32_t));
        audio_stop_voice();
        audio_stop_chiptune();
        audio_stop_cosmic_sfx();
        audio_stream_reset();
        response_received_samples = 0;
        resp_is_receiving = true;
        resp_is_ready = false;
        last_resp_chunk_rx_ms = ctx->now;
    }
    if (ack) {
        response_progress_ms = ctx->now;
        acknowledge_response(hdr, ctx->addr, ctx->port);
    }
}

static void handle_rx_resp_chunk(const net_packet_header_t *hdr,
                                 const uint8_t *payload,
                                 void *user_ctx) {
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (payload == NULL || !is_authorized_response_source(ctx)) return;
    if (!response_gate_matches(&response_gate, hdr->request_id)) return;

    bool ack = false;
    response_gate_t candidate = response_gate;
    int accepted = response_gate_chunk(&candidate, hdr->request_id, hdr->seq_num);
    if (accepted == 1) {
        ack = true;
    } else if (accepted == 2) {
        int16_t chunk[NET_MAX_PAYLOAD_SIZE / 2];
        memcpy(chunk, payload, hdr->payload_len);
        size_t count = hdr->payload_len / 2;
        if (audio_stream_push(chunk, count)) {
            response_gate = candidate;
            response_received_samples += (uint32_t)count;
            last_resp_chunk_rx_ms = ctx->now;
            ack = true;
        }
    }
    if (ack) {
        response_progress_ms = ctx->now;
        acknowledge_response(hdr, ctx->addr, ctx->port);
    }
}

static void handle_rx_resp_end(const net_packet_header_t *hdr,
                               const uint8_t *payload,
                               void *user_ctx) {
    (void)payload;
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (!is_authorized_response_source(ctx)) return;
    if (!response_gate_matches(&response_gate, hdr->request_id)) return;

    if (!response_gate.ended && response_gate.started) {
        bool ack = response_gate_end(&response_gate, hdr->request_id, hdr->seq_num);
        audio_stream_finish();
        resp_is_receiving = false;
        resp_is_ready = true;
        if (ack) {
            response_progress_ms = ctx->now;
            acknowledge_response(hdr, ctx->addr, ctx->port);
        }
    }
}

static void handle_rx_progress(const net_packet_header_t *hdr,
                               const uint8_t *payload,
                               void *user_ctx) {
    (void)payload;
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (!is_authorized_response_source(ctx)) return;
    if (response_gate_matches(&response_gate, hdr->request_id)) {
        response_progress_ms = ctx->now;
    }
}

static void handle_rx_error(const net_packet_header_t *hdr,
                            const uint8_t *payload,
                            void *user_ctx) {
    (void)payload;
    const net_rx_context_t *ctx = (const net_rx_context_t *)user_ctx;
    if (!is_authorized_response_source(ctx)) return;
    if (response_gate_matches(&response_gate, hdr->request_id)) {
        response_failed = true;
        response_gate_cancel(&response_gate);
        audio_stop_all();
        resp_is_receiving = resp_is_ready = false;
    }
}

static const net_dispatch_entry_t NET_CLIENT_RX_DISPATCH_TABLE[] = {
    { NET_MSG_APP_COMMAND,      handle_rx_app_command   },
    { NET_MSG_APP_TIME_SYNC,    handle_rx_app_time_sync },
    { NET_MSG_CANCEL,           handle_rx_cancel        },
    { NET_MSG_CATALOG_RESP,     handle_rx_catalog_resp  },
    { NET_MSG_HEARTBEAT_ACK,    handle_rx_heartbeat     },
    { NET_MSG_RESP_TEXT,        handle_rx_resp_text     },
    { NET_MSG_RESP_AUDIO_START, handle_rx_resp_start    },
    { NET_MSG_RESP_AUDIO_CHUNK, handle_rx_resp_chunk    },
    { NET_MSG_RESP_AUDIO_END,   handle_rx_resp_end      },
    { NET_MSG_PROGRESS,         handle_rx_progress      },
    { NET_MSG_ERROR,            handle_rx_error         }
};

static void net_client_udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                         const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;
    if (!p) return;

    uint8_t raw_pkt[sizeof(net_packet_header_t) + NET_MAX_PAYLOAD_SIZE];
    if (p->tot_len < sizeof(net_packet_header_t) || p->tot_len > sizeof(raw_pkt)) {
        pbuf_free(p);
        return;
    }
    if (pbuf_copy_partial(p, raw_pkt, p->tot_len, 0) != p->tot_len) {
        pbuf_free(p);
        return;
    }

    net_rx_context_t rx_ctx = {
        .addr = addr,
        .port = port,
        .now  = to_ms_since_boot(get_absolute_time())
    };
    (void)net_dispatch_process_packet(
        NET_CLIENT_RX_DISPATCH_TABLE,
        sizeof(NET_CLIENT_RX_DISPATCH_TABLE) / sizeof(NET_CLIENT_RX_DISPATCH_TABLE[0]),
        raw_pkt,
        p->tot_len,
        &rx_ctx
    );
    pbuf_free(p);
}

static uint32_t last_reconnect_attempt_ms = 0;

void net_client_init(void) {
    active_ap_profile = 0;
    ap_connect_failures = 0;
    active_server_port = get_current_server_port();
    ipaddr_aton(get_current_server_ip_str(), &server_ip_addr);
    ipaddr_aton(strlen(BRAIN_FALLBACK_SERVER_IP) > 0 ? BRAIN_FALLBACK_SERVER_IP : BRAIN_SERVER_IP, &pi5_server_ip_addr);
    memcpy(&active_catalog, &DEFAULT_PHONE_CATALOG, sizeof(active_catalog));
    active_host_type = HOST_TYPE_PHONE;
    strncpy(active_host_name, DEFAULT_PHONE_CATALOG.host_name, sizeof(active_host_name));
    request_counter = get_rand_32(); /* Reboot must not reuse a still-running worker's ID. */

#if HAVE_WIFI_CREDENTIALS
    const char *ssid = get_current_ssid();
    if (strlen(ssid) > 0) {
        // CYW43 architecture is already initialized in main.c; enable station mode
        cyw43_arch_enable_sta_mode();

        // Create UDP PCB inside the lwIP lock
        cyw43_arch_lwip_begin();
        if (udp_client_pcb != NULL) {
            udp_remove(udp_client_pcb);
            udp_client_pcb = NULL;
        }
        udp_client_pcb = udp_new();
        if (udp_client_pcb != NULL) {
            ip_set_option(udp_client_pcb, SOF_BROADCAST);
            udp_bind(udp_client_pcb, IP_ADDR_ANY, CELESTIAL_NET_DEFAULT_PORT);
            udp_recv(udp_client_pcb, net_client_udp_recv_callback, NULL);
        }
        cyw43_arch_lwip_end();

        // Start non-blocking Wi-Fi association (cyw43_wifi_join acquires CYW43_THREAD_ENTER internally)
        int err = cyw43_arch_wifi_connect_async(ssid, get_current_password(), CYW43_AUTH_WPA2_MIXED_PSK);

        if (err == 0) {
            current_status = NET_STATUS_CONNECTING;
        } else {
            current_status = NET_STATUS_ERROR;
        }
        last_reconnect_attempt_ms = to_ms_since_boot(get_absolute_time());
    }
#else
    current_status = NET_STATUS_DISCONNECTED;
#endif
}

void net_client_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_status_check_ms < 500) {
        return;
    }
    last_status_check_ms = now;

    cyw43_arch_lwip_begin();
    bool stalled = resp_is_receiving && (now - last_resp_chunk_rx_ms > 10000);
    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link_status == CYW43_LINK_UP) {
        struct netif *nif = &cyw43_state.netif[CYW43_ITF_STA];
        ip4addr_ntoa_r(netif_ip4_addr(nif), current_ip_str, sizeof(current_ip_str));
    }
    cyw43_arch_lwip_end();

    if (stalled) { net_client_cancel(); response_failed = true; }

    if (link_status == CYW43_LINK_UP) {
        ap_connect_failures = 0;
        if (current_status != NET_STATUS_CONNECTED) {
            current_status = NET_STATUS_CONNECTED;
            active_server_port = get_current_server_port();
            ipaddr_aton(get_current_server_ip_str(), &server_ip_addr);
        }
        if (udp_client_pcb == NULL) {
            cyw43_arch_lwip_begin();
            udp_client_pcb = udp_new();
            if (udp_client_pcb != NULL) {
                ip_set_option(udp_client_pcb, SOF_BROADCAST);
                udp_bind(udp_client_pcb, IP_ADDR_ANY, CELESTIAL_NET_DEFAULT_PORT);
                udp_recv(udp_client_pcb, net_client_udp_recv_callback, NULL);
            }
            cyw43_arch_lwip_end();
        }

        // When targeting PHONE host on the mobile hotspot profile (active_ap_profile == 0) without a paired companion yet,
        // dynamically resolve server IP to DHCP default gateway (phone AP IP).
        // Never overwrite server_ip_addr with the home router IP when on active_ap_profile == 1 (home Wi-Fi / Pi 5)!
        if (active_ap_profile == 0 && (active_host_type == HOST_TYPE_PHONE || preferred_host == HOST_TYPE_PHONE) && !has_paired_companion) {
            ip4_addr_t gw_ip;
            bool has_gw = false;
            cyw43_arch_lwip_begin();
            struct netif *nif = &cyw43_state.netif[CYW43_ITF_STA];
            const ip4_addr_t *gw = netif_ip4_gw(nif);
            if (gw != NULL && gw->addr != 0) {
                gw_ip = *gw;
                has_gw = true;
            }
            cyw43_arch_lwip_end();
            if (has_gw && !ip_addr_cmp(&server_ip_addr, &gw_ip)) {
                ip_addr_copy_from_ip4(server_ip_addr, gw_ip);
                printf("[NET] Target server IP updated to gateway (Phone AP): %s\n", ip4addr_ntoa(&server_ip_addr));
            }
        }

        // Periodic heartbeat ping to verify Brain Server is alive (every 1.0 s)
        if (now - last_heartbeat_tx_ms >= 1000) {
            last_heartbeat_tx_ms = now;
            net_client_ping_server();
        }

        // Host auto-failover watchdog (only in auto mode):
        if (preferred_host == HOST_TYPE_NONE && active_host_type == HOST_TYPE_PI5) {
            if (now - last_pi5_heartbeat_rx_ms > 4500) {
                // Pi 5 has gone silent!
                if (has_phone_catalog && (now - last_phone_heartbeat_rx_ms < 15000)) {
                    active_host_type = HOST_TYPE_PHONE;
                    server_ip_addr = companion_ip_addr;
                    active_server_port = CELESTIAL_NET_DEFAULT_PORT;
                    last_server_heartbeat_rx_ms = last_phone_heartbeat_rx_ms;
                    memcpy(&active_catalog, &phone_catalog_cache, sizeof(active_catalog));
                    strncpy(active_host_name, phone_catalog_cache.host_name, sizeof(active_host_name));
                    printf("[NET] FAILOVER: Pi 5 heartbeat timed out. Switched AI Host to ANDROID PHONE (%s)\n",
                           ip4addr_ntoa(&server_ip_addr));
                } else {
                    active_host_type = HOST_TYPE_NONE;
                    strncpy(active_host_name, "STANDBY", sizeof(active_host_name));
                }
            }
        }

        // Periodic dynamic model catalog query every 4.0 s
        if (now - last_catalog_req_ms >= 4000) {
            last_catalog_req_ms = now;
            net_client_request_catalog();
        }

    } else {
        if (link_status == CYW43_LINK_BADAUTH || link_status == CYW43_LINK_NONET) {
            current_status = NET_STATUS_ERROR;
        } else {
            current_status = NET_STATUS_CONNECTING;
        }

#if HAVE_WIFI_CREDENTIALS
        // Allow up to 15 seconds for Wi-Fi association and DHCP negotiation.
        // If SSID is absent (CYW43_LINK_NONET) on Hotspot profile 0, fail over after 6 seconds.
        bool is_in_progress = (link_status == CYW43_LINK_JOIN || link_status == CYW43_LINK_NOIP);
        uint32_t timeout_ms = is_in_progress ? 14000 : (active_ap_profile == 0 ? 6000 : 10000);
        int max_failures = (active_ap_profile == 0) ? 1 : 2;

        if (now - last_reconnect_attempt_ms > timeout_ms) {
            last_reconnect_attempt_ms = now;
            ap_connect_failures++;
            if (strlen(WIFI_FALLBACK_SSID) > 0 && ap_connect_failures >= max_failures) {
                // Toggle between primary (0 = Hotspot) and fallback (1 = Home Wi-Fi)
                active_ap_profile = (active_ap_profile == 0) ? 1 : 0;
                ap_connect_failures = 0;
                active_server_port = get_current_server_port();
                ipaddr_aton(get_current_server_ip_str(), &server_ip_addr);
                printf("[NET] Failing over to AP profile %d: [%s] (Target Server: %s:%d)\n",
                       active_ap_profile, get_current_ssid(), get_current_server_ip_str(), (int)active_server_port);
            }
            const char *ssid = get_current_ssid();
            if (strlen(ssid) > 0) {
                printf("[NET] Connecting to Wi-Fi SSID [%s] (attempt %d)...\n", ssid, ap_connect_failures + 1);
                cyw43_arch_lwip_begin();
                cyw43_arch_wifi_connect_async(ssid, get_current_password(), CYW43_AUTH_WPA2_MIXED_PSK);
                cyw43_arch_lwip_end();
            }
        }
#endif
    }
}

net_status_t net_client_get_status(void) {
    return current_status;
}

bool net_client_is_connected(void) {
    return (current_status == NET_STATUS_CONNECTED);
}

bool net_client_is_server_ready(void) {
    if (current_status != NET_STATUS_CONNECTED) {
        return false;
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (active_host_type == HOST_TYPE_PHONE) {
        return (last_phone_heartbeat_rx_ms > 0 && (now - last_phone_heartbeat_rx_ms <= 15000));
    } else if (active_host_type == HOST_TYPE_PI5) {
        return (last_pi5_heartbeat_rx_ms > 0 && (now - last_pi5_heartbeat_rx_ms <= 15000));
    }
    return (last_server_heartbeat_rx_ms > 0 && (now - last_server_heartbeat_rx_ms <= 15000));
}

const char* net_client_get_ip_str(void) {
    return current_ip_str;
}

const char* net_client_get_active_ssid(void) {
    return get_current_ssid();
}

const char* net_client_get_server_ip_str(void) {
    if (active_host_type != HOST_TYPE_NONE) {
        return ip4addr_ntoa(&server_ip_addr);
    }
    return get_current_server_ip_str();
}

uint16_t net_client_get_server_port(void) {
    return active_server_port;
}

static bool send_udp_packet(uint8_t msg_type, const void *payload, size_t payload_len) {
    if (udp_client_pcb == NULL || current_status != NET_STATUS_CONNECTED) {
        return false;
    }

    size_t total_len = sizeof(net_packet_header_t) + payload_len;
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
    if (p == NULL) {
        cyw43_arch_lwip_end();
        return false;
    }

    net_packet_header_t *hdr = (net_packet_header_t *)p->payload;
    hdr->magic = CELESTIAL_NET_MAGIC;
    hdr->version = CELESTIAL_NET_VERSION;
    hdr->msg_type = msg_type;
    hdr->seq_num = tx_seq_counter++;
    hdr->payload_len = (uint16_t)payload_len;
    hdr->request_id = msg_type == NET_MSG_HEARTBEAT ? 0 : response_gate.request_id;

    if (payload != NULL && payload_len > 0) {
        memcpy((uint8_t *)p->payload + sizeof(net_packet_header_t), payload, payload_len);
    }

    uint16_t target_port = (active_host_type == HOST_TYPE_PI5) ? pi5_server_port : CELESTIAL_NET_DEFAULT_PORT;
    active_server_port = target_port;
    err_t err = udp_sendto(udp_client_pcb, p, &server_ip_addr, target_port);
    pbuf_free(p);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) {
        printf("[NET] udp_sendto(type=0x%02X, len=%u, to=%s:%d) FAILED err=%d\n",
               msg_type, (unsigned)payload_len, ip4addr_ntoa(&server_ip_addr), (int)target_port, (int)err);
    }

    return (err == ERR_OK);
}

bool net_client_ping_server(void) {
    // 1. Unicast ping to current active server
    send_udp_packet(NET_MSG_HEARTBEAT, NULL, 0);

    // 2. If active server is NOT Pi 5, also probe Pi 5 to detect when it powers back on
    if (active_host_type != HOST_TYPE_PI5 && udp_client_pcb != NULL && current_status == NET_STATUS_CONNECTED) {
        size_t total_len = sizeof(net_packet_header_t);
        cyw43_arch_lwip_begin();
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
        if (p != NULL) {
            net_packet_header_t *hdr = (net_packet_header_t *)p->payload;
            hdr->magic = CELESTIAL_NET_MAGIC;
            hdr->version = CELESTIAL_NET_VERSION;
            hdr->msg_type = NET_MSG_HEARTBEAT;
            hdr->seq_num = tx_seq_counter++;
            hdr->payload_len = 0;
            hdr->request_id = 0;
            udp_sendto(udp_client_pcb, p, &pi5_server_ip_addr, pi5_server_port);
            pbuf_free(p);
        }
        cyw43_arch_lwip_end();
    }

    // 2b. If active server is NOT Phone and we have a paired companion, probe the phone
    if (active_host_type != HOST_TYPE_PHONE && has_paired_companion && udp_client_pcb != NULL && current_status == NET_STATUS_CONNECTED) {
        size_t total_len = sizeof(net_packet_header_t);
        cyw43_arch_lwip_begin();
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
        if (p != NULL) {
            net_packet_header_t *hdr = (net_packet_header_t *)p->payload;
            hdr->magic = CELESTIAL_NET_MAGIC;
            hdr->version = CELESTIAL_NET_VERSION;
            hdr->msg_type = NET_MSG_HEARTBEAT;
            hdr->seq_num = tx_seq_counter++;
            hdr->payload_len = 0;
            hdr->request_id = 0;
            udp_sendto(udp_client_pcb, p, &companion_ip_addr, CELESTIAL_NET_DEFAULT_PORT);
            pbuf_free(p);
        }
        cyw43_arch_lwip_end();
    }

    // 3. Broadcast Discovery Beacon to subnet (255.255.255.255:5005) so Companion App discovers staff
    if (udp_client_pcb != NULL && current_status == NET_STATUS_CONNECTED) {
        size_t total_len = sizeof(net_packet_header_t);
        cyw43_arch_lwip_begin();
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
        if (p != NULL) {
            net_packet_header_t *hdr = (net_packet_header_t *)p->payload;
            hdr->magic = CELESTIAL_NET_MAGIC;
            hdr->version = CELESTIAL_NET_VERSION;
            hdr->msg_type = NET_MSG_HEARTBEAT;
            hdr->seq_num = tx_seq_counter++;
            hdr->payload_len = 0;
            hdr->request_id = 0;
            ip_addr_t bcast;
            ip_addr_set_ip4_u32(&bcast, IPADDR_BROADCAST);
            udp_sendto(udp_client_pcb, p, &bcast, CELESTIAL_NET_DEFAULT_PORT);
            pbuf_free(p);
        }
        cyw43_arch_lwip_end();
    }
    return true;
}

bool net_client_send_query_start(uint8_t lang_code, uint8_t model_preset) {
    net_client_cancel();
    cyw43_arch_lwip_begin();
    if (++request_counter == 0) ++request_counter;
    response_gate_begin(&response_gate, request_counter);
    resp_is_ready = resp_is_receiving = response_failed = false;
    resp_text_buffer[0] = '\0';
    response_progress_ms = to_ms_since_boot(get_absolute_time());
    cyw43_arch_lwip_end();
    uint8_t payload[2] = { lang_code, model_preset };
    bool sent = send_udp_packet(NET_MSG_QUERY_START, payload, sizeof payload);
    if (sent) send_udp_packet(NET_MSG_QUERY_START, payload, sizeof payload);
    else { net_client_cancel(); response_failed = true; }
    return sent;
}

bool net_client_send_audio_chunk(const int16_t *samples, size_t sample_count) {
    if (samples == NULL || sample_count == 0) {
        return true;
    }

    size_t offset = 0;
    while (offset < sample_count) {
        size_t chunk_samples = sample_count - offset;
        size_t max_samples_per_pkt = NET_MAX_PAYLOAD_SIZE / sizeof(int16_t);
        if (chunk_samples > max_samples_per_pkt) {
            chunk_samples = max_samples_per_pkt;
        }

        size_t bytes = chunk_samples * sizeof(int16_t);
        if (!send_udp_packet(NET_MSG_AUDIO_CHUNK, &samples[offset], bytes)) {
            return false;
        }
        offset += chunk_samples;
    }
    return true;
}

bool net_client_send_query_end(void) {
    bool sent = send_udp_packet(NET_MSG_QUERY_END, NULL, 0);
    if (sent) {
        send_udp_packet(NET_MSG_QUERY_END, NULL, 0);
        send_udp_packet(NET_MSG_QUERY_END, NULL, 0);
    }
    return sent;
}

void net_client_cancel(void) {
    cyw43_arch_lwip_begin();
    response_gate_cancel(&response_gate);
    cyw43_arch_lwip_end();
    send_udp_packet(NET_MSG_CANCEL, NULL, 0);
    resp_is_receiving = false;
    resp_is_ready = false;
    net_cancel_flag = false;
}

bool net_client_check_and_clear_cancel(void) {
    bool cancelled = net_cancel_flag;
    net_cancel_flag = false;
    return cancelled;
}

bool net_client_is_receiving_response(void) {
    return resp_is_receiving;
}

bool net_client_has_response_ready(void) {
    return resp_is_ready;
}

void net_client_consume_response(void) {
    resp_is_ready = false;
    resp_is_receiving = false;
    /* Keep the completed gate briefly so a retransmitted END can still be ACKed. */
}

const char* net_client_get_response_text(void) {
    return resp_text_buffer;
}

bool net_client_response_failed(void) { return response_failed; }
uint32_t net_client_response_progress_ms(void) { return response_progress_ms; }

bool net_client_poll_remote_command(remote_cmd_t *cmd) {
    if (remote_cmd_tail == remote_cmd_head) {
        return false;
    }
    *cmd = remote_cmd_queue[remote_cmd_tail];
    remote_cmd_tail = (remote_cmd_tail + 1) % REMOTE_CMD_QUEUE_LEN;
    return true;
}

bool net_client_has_paired_companion(void) {
    return has_paired_companion;
}

bool net_client_has_unsolicited_text(void) {
    return unsolicited_text_ready;
}

void net_client_clear_unsolicited_text(void) {
    unsolicited_text_ready = false;
}

static void send_raw_udp_to(uint8_t msg_type, const void *payload, size_t payload_len,
                            const ip_addr_t *dst_ip, uint16_t dst_port) {
    if (udp_client_pcb == NULL || dst_ip == NULL || dst_ip->addr == 0) {
        return;
    }
    size_t total_len = sizeof(net_packet_header_t) + payload_len;
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
    if (p != NULL) {
        net_packet_header_t *hdr = (net_packet_header_t *)p->payload;
        hdr->magic = CELESTIAL_NET_MAGIC;
        hdr->version = CELESTIAL_NET_VERSION;
        hdr->msg_type = msg_type;
        hdr->seq_num = tx_seq_counter++;
        hdr->payload_len = (uint16_t)payload_len;
        hdr->request_id = 0;
        if (payload != NULL && payload_len > 0) {
            memcpy((uint8_t *)p->payload + sizeof(*hdr), payload, payload_len);
        }
        udp_sendto(udp_client_pcb, p, dst_ip, dst_port);
        pbuf_free(p);
    }
}

void net_client_send_telemetry(uint8_t mode, uint8_t state, uint8_t volume, uint8_t anim, uint8_t clock_style,
                               uint8_t lang, uint8_t host_type, uint8_t pref_host,
                               uint8_t ai_model, uint8_t mic_level, uint8_t cosmos_zoom, uint8_t is_audio_playing) {
    if (udp_client_pcb == NULL || current_status != NET_STATUS_CONNECTED) {
        return;
    }
    uint8_t payload[12] = { mode, state, volume, anim, clock_style, lang, host_type, pref_host,
                            ai_model, mic_level, cosmos_zoom, is_audio_playing };
    cyw43_arch_lwip_begin();
    if (has_paired_companion) {
        send_raw_udp_to(NET_MSG_APP_TELEMETRY, payload, sizeof(payload), &companion_ip_addr, CELESTIAL_NET_DEFAULT_PORT);
    } else {
        ip_addr_t bcast;
        ip_addr_set_ip4_u32(&bcast, IPADDR_BROADCAST);
        send_raw_udp_to(NET_MSG_APP_TELEMETRY, payload, sizeof(payload), &bcast, CELESTIAL_NET_DEFAULT_PORT);
        if (server_ip_addr.addr != 0) {
            send_raw_udp_to(NET_MSG_APP_TELEMETRY, payload, sizeof(payload), &server_ip_addr, active_server_port);
        }
    }
    cyw43_arch_lwip_end();
}

bool net_client_request_catalog(void) {
    if (udp_client_pcb == NULL || current_status != NET_STATUS_CONNECTED) {
        return false;
    }
    cyw43_arch_lwip_begin();
    // 1. Send query to Pi 5 (fresh pbuf per destination so etharp_query never sees p->ref > 1)
    send_raw_udp_to(NET_MSG_CATALOG_REQ, NULL, 0, &pi5_server_ip_addr, pi5_server_port);

    // 2. If phone companion is known, send query to phone
    if (has_paired_companion) {
        send_raw_udp_to(NET_MSG_CATALOG_REQ, NULL, 0, &companion_ip_addr, CELESTIAL_NET_DEFAULT_PORT);
    }

    // 3. If in fallback hotspot profile and fallback IP differs from pi5_server_ip_addr, query fallback gateway
    if (active_ap_profile == 1) {
        ip_addr_t fb_ip;
        ipaddr_aton(BRAIN_FALLBACK_SERVER_IP, &fb_ip);
        if (!ip_addr_cmp(&fb_ip, &pi5_server_ip_addr) || BRAIN_FALLBACK_SERVER_PORT != pi5_server_port) {
            send_raw_udp_to(NET_MSG_CATALOG_REQ, NULL, 0, &fb_ip, BRAIN_FALLBACK_SERVER_PORT);
        }
    }
    cyw43_arch_lwip_end();
    return true;
}

net_host_type_t net_client_get_host_type(void) {
    return active_host_type;
}

const char* net_client_get_host_name(void) {
    return active_host_name;
}

uint8_t net_client_get_model_count(void) {
    if (active_catalog.model_count == 0) return 1;
    return active_catalog.model_count;
}

const char* net_client_get_model_name(uint8_t idx) {
    if (active_catalog.model_count == 0) {
        if (idx == 0) return DEFAULT_PI5_CATALOG.models[0].name;
        return "UNKNOWN";
    }
    if (idx >= active_catalog.model_count) return "UNKNOWN";
    return active_catalog.models[idx].name;
}

const char* net_client_get_model_tag(uint8_t idx) {
    if (active_catalog.model_count == 0) {
        if (idx == 0) return DEFAULT_PI5_CATALOG.models[0].tag;
        return "???";
    }
    if (idx >= active_catalog.model_count) return "???";
    return active_catalog.models[idx].tag;
}

uint8_t net_client_get_model_theme(uint8_t idx) {
    if (idx >= active_catalog.model_count) return 0;
    return active_catalog.models[idx].theme_idx;
}

uint8_t net_client_get_model_id(uint8_t idx) {
    if (idx >= active_catalog.model_count) return 0;
    return active_catalog.models[idx].model_id;
}

void net_client_set_preferred_host(net_host_type_t pref) {
    preferred_host = pref;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (pref == HOST_TYPE_PHONE) {
        active_host_type = HOST_TYPE_PHONE;
        if (has_paired_companion) {
            server_ip_addr = companion_ip_addr;
            active_server_port = CELESTIAL_NET_DEFAULT_PORT;
        } else if (active_ap_profile == 1 && strlen(BRAIN_FALLBACK_SERVER_IP) > 0) {
            ipaddr_aton(BRAIN_FALLBACK_SERVER_IP, &server_ip_addr);
            active_server_port = BRAIN_FALLBACK_SERVER_PORT;
        } else {
            // Default hotspot gateway IP when phone tethering is active
            ipaddr_aton("192.168.43.1", &server_ip_addr);
            active_server_port = CELESTIAL_NET_DEFAULT_PORT;
        }
        if (has_phone_catalog) {
            memcpy(&active_catalog, &phone_catalog_cache, sizeof(active_catalog));
            strncpy(active_host_name, phone_catalog_cache.host_name, sizeof(active_host_name));
        } else {
            memcpy(&active_catalog, &DEFAULT_PHONE_CATALOG, sizeof(active_catalog));
            strncpy(active_host_name, DEFAULT_PHONE_CATALOG.host_name, sizeof(active_host_name));
        }
        printf("[NET] Host preference locked to PHONE BRAIN (%s:%d)\n",
               ip4addr_ntoa(&server_ip_addr), (int)active_server_port);
    } else if (pref == HOST_TYPE_PI5) {
        active_host_type = HOST_TYPE_PI5;
        server_ip_addr = pi5_server_ip_addr;
        active_server_port = pi5_server_port;
        if (has_pi5_catalog) {
            memcpy(&active_catalog, &pi5_catalog_cache, sizeof(active_catalog));
            strncpy(active_host_name, pi5_catalog_cache.host_name, sizeof(active_host_name));
        } else {
            memcpy(&active_catalog, &DEFAULT_PI5_CATALOG, sizeof(active_catalog));
            strncpy(active_host_name, DEFAULT_PI5_CATALOG.host_name, sizeof(active_host_name));
        }
        printf("[NET] Host preference locked to PI 5 BRAIN\n");
    } else {
        printf("[NET] Host preference set to AUTO MESH\n");
        if (now - last_pi5_heartbeat_rx_ms < 4000) {
            active_host_type = HOST_TYPE_PI5;
            server_ip_addr = pi5_server_ip_addr;
            active_server_port = pi5_server_port;
            if (has_pi5_catalog) {
                memcpy(&active_catalog, &pi5_catalog_cache, sizeof(active_catalog));
                strncpy(active_host_name, pi5_catalog_cache.host_name, sizeof(active_host_name));
            } else {
                memcpy(&active_catalog, &DEFAULT_PI5_CATALOG, sizeof(active_catalog));
                strncpy(active_host_name, DEFAULT_PI5_CATALOG.host_name, sizeof(active_host_name));
            }
        } else if (has_phone_catalog || has_paired_companion) {
            active_host_type = HOST_TYPE_PHONE;
            if (has_paired_companion) {
                server_ip_addr = companion_ip_addr;
                active_server_port = companion_port ? companion_port : BRAIN_SERVER_PORT;
            }
            if (has_phone_catalog) {
                memcpy(&active_catalog, &phone_catalog_cache, sizeof(active_catalog));
                strncpy(active_host_name, phone_catalog_cache.host_name, sizeof(active_host_name));
            } else {
                memcpy(&active_catalog, &DEFAULT_PHONE_CATALOG, sizeof(active_catalog));
                strncpy(active_host_name, DEFAULT_PHONE_CATALOG.host_name, sizeof(active_host_name));
            }
        } else {
            memcpy(&active_catalog, &DEFAULT_PI5_CATALOG, sizeof(active_catalog));
            strncpy(active_host_name, DEFAULT_PI5_CATALOG.host_name, sizeof(active_host_name));
        }
    }
    net_client_request_catalog();
}

net_host_type_t net_client_get_preferred_host(void) {
    return preferred_host;
}

