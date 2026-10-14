#include "net_client.h"
#include "net_protocol.h"
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
static u16_t companion_port = 0;
static bool has_paired_companion = false;
static remote_cmd_t pending_remote_cmd = {0};

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

static uint32_t last_server_heartbeat_rx_ms = 0;
static uint32_t last_heartbeat_tx_ms = 0;

static uint32_t last_resp_chunk_rx_ms = 0;
static response_gate_t response_gate;
static uint32_t request_counter;
static volatile bool response_failed;
static volatile uint32_t response_progress_ms;
static uint32_t response_expected_samples, response_received_samples;

/* Called only inside the lwIP callback, which already owns the SDK lock. */
static void acknowledge_response(const net_packet_header_t *received) {
    struct pbuf *ack = pbuf_alloc(PBUF_TRANSPORT, sizeof(*received) + 1, PBUF_RAM);
    if (!ack) return;
    net_packet_header_t hdr = { CELESTIAL_NET_MAGIC, CELESTIAL_NET_VERSION,
        NET_MSG_ACK, received->seq_num, 1, received->request_id };
    memcpy(ack->payload, &hdr, sizeof hdr);
    ((uint8_t *)ack->payload)[sizeof hdr] = received->msg_type;
    udp_sendto(udp_client_pcb, ack, &server_ip_addr, active_server_port);
    pbuf_free(ack);
}

static void net_client_udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                         const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;
    if (!p) return;
    net_packet_header_t hdr;
    if (p->tot_len < sizeof hdr || pbuf_copy_partial(p, &hdr, sizeof hdr, 0) != sizeof hdr ||
        hdr.magic != CELESTIAL_NET_MAGIC || hdr.version != CELESTIAL_NET_VERSION ||
        hdr.payload_len > NET_MAX_PAYLOAD_SIZE || p->tot_len != sizeof hdr + hdr.payload_len) {
        pbuf_free(p);
        return;
    }

    // Companion App Remote Control (Signed with CELESTIAL_AUTH_SECRET)
    if (hdr.msg_type == NET_MSG_APP_COMMAND) {
        if (hdr.payload_len >= 6) {
            uint8_t payload[8];
            pbuf_copy_partial(p, payload, 6, sizeof hdr);
            uint32_t token = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                            ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
            uint32_t expected = compute_auth_token(CELESTIAL_AUTH_SECRET);
            if (strlen(CELESTIAL_AUTH_SECRET) == 0 || token == expected) {
                companion_ip_addr = *addr;
                companion_port = port;
                has_paired_companion = true;
                pending_remote_cmd.cmd_code = payload[4];
                pending_remote_cmd.param = payload[5];
                pending_remote_cmd.has_command = true;
                printf("[SECURITY] Accepted authorized command '%c' (param %d) from %s:%d\n",
                       payload[4], payload[5], ip4addr_ntoa(addr), (int)port);
            } else {
                printf("[SECURITY] Rejected unauthorized command from %s:%d (auth token 0x%08X != expected 0x%08X)\n",
                       ip4addr_ntoa(addr), (int)port, (unsigned int)token, (unsigned int)expected);
            }
        }
        pbuf_free(p);
        return;
    }

    if (hdr.msg_type == NET_MSG_APP_TIME_SYNC) {
        if (hdr.payload_len >= 12) {
            uint8_t payload[12];
            pbuf_copy_partial(p, payload, 12, sizeof hdr);
            uint32_t token = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                            ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
            uint32_t expected = compute_auth_token(CELESTIAL_AUTH_SECRET);
            if (strlen(CELESTIAL_AUTH_SECRET) == 0 || token == expected) {
                companion_ip_addr = *addr;
                companion_port = port;
                has_paired_companion = true;
                pending_remote_cmd.cmd_code = 'X';
                pending_remote_cmd.val32 = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) |
                                          ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
                pending_remote_cmd.val32_extra = (uint32_t)payload[8] | ((uint32_t)payload[9] << 8) |
                                                ((uint32_t)payload[10] << 16) | ((uint32_t)payload[11] << 24);
                pending_remote_cmd.has_command = true;
                printf("[SECURITY] Accepted time sync from %s:%d\n", ip4addr_ntoa(addr), (int)port);
            }
        }
        pbuf_free(p);
        return;
    }

    if (!ip_addr_cmp(addr, &server_ip_addr) || port != active_server_port) {
        pbuf_free(p);
        return;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (hdr.msg_type == NET_MSG_HEARTBEAT && hdr.request_id == 0) {
        last_server_heartbeat_rx_ms = now;
        if (hdr.payload_len >= (uint16_t)sizeof(uint32_t)) {
            uint32_t server_epoch = 0;
            pbuf_copy_partial(p, &server_epoch, sizeof(uint32_t), sizeof(hdr));
            if (server_epoch > 1700000000) {
                extern void set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis);
                set_current_time_epoch(server_epoch, 0);
            }
        }
    } else if (response_gate_matches(&response_gate, hdr.request_id)) {
        bool ack = false;
        switch (hdr.msg_type) {
        case NET_MSG_PROGRESS:
            response_progress_ms = now;
            break;
        case NET_MSG_ERROR:
            response_failed = true;
            response_gate_cancel(&response_gate);
            audio_stop_all();
            resp_is_receiving = resp_is_ready = false;
            break;
        case NET_MSG_RESP_TEXT:
            if (!response_gate.ended && hdr.payload_len < sizeof(resp_text_buffer)) {
                pbuf_copy_partial(p, resp_text_buffer, hdr.payload_len, sizeof hdr);
                resp_text_buffer[hdr.payload_len] = '\0';
                ack = true;
            }
            break;
        case NET_MSG_RESP_START:
            if (hdr.payload_len == sizeof(uint32_t)) {
                bool first = !response_gate.started;
                ack = response_gate_start(&response_gate, hdr.request_id);
                if (ack && first) {
                    pbuf_copy_partial(p, &response_expected_samples, sizeof(uint32_t), sizeof hdr);
                    audio_stream_reset();
                    response_received_samples = 0;
                    resp_is_receiving = true;
                    resp_is_ready = false;
                    last_resp_chunk_rx_ms = now;
                }
            }
            break;
        case NET_MSG_RESP_CHUNK:
            if (hdr.payload_len && !(hdr.payload_len & 1)) {
                response_gate_t candidate = response_gate;
                int accepted = response_gate_chunk(&candidate, hdr.request_id, hdr.seq_num);
                if (accepted == 1) ack = true;
                else if (accepted == 2) {
                    int16_t chunk[NET_MAX_PAYLOAD_SIZE / 2];
                    pbuf_copy_partial(p, chunk, hdr.payload_len, sizeof hdr);
                    size_t count = hdr.payload_len / 2;
                    if (response_received_samples + count <= response_expected_samples &&
                        audio_stream_push(chunk, count)) {
                        response_gate = candidate;
                        response_received_samples += count;
                        last_resp_chunk_rx_ms = now;
                        ack = true;
                    }
                }
            }
            break;
        case NET_MSG_RESP_END:
            if (response_received_samples == response_expected_samples) {
                bool first = !response_gate.ended;
                ack = response_gate_end(&response_gate, hdr.request_id, hdr.seq_num);
                if (ack && first) {
                    audio_stream_finish();
                    resp_is_receiving = false;
                    resp_is_ready = true;
                }
            }
            break;
        default: break;
        }
        if (ack) {
            response_progress_ms = now;
            acknowledge_response(&hdr);
        }
    }
    pbuf_free(p);
}

void net_client_init(void) {
    active_ap_profile = 0;
    ap_connect_failures = 0;
    active_server_port = get_current_server_port();
    ipaddr_aton(get_current_server_ip_str(), &server_ip_addr);
    request_counter = get_rand_32(); /* Reboot must not reuse a still-running worker's ID. */

#if HAVE_WIFI_CREDENTIALS
    const char *ssid = get_current_ssid();
    if (strlen(ssid) > 0) {
        // CYW43 architecture is already initialized in main.c; enable station mode
        cyw43_arch_enable_sta_mode();

        // Create UDP PCB
        cyw43_arch_lwip_begin();
        if (udp_client_pcb != NULL) {
            udp_remove(udp_client_pcb);
            udp_client_pcb = NULL;
        }
        udp_client_pcb = udp_new();
        if (udp_client_pcb != NULL) {
            udp_bind(udp_client_pcb, IP_ADDR_ANY, CELESTIAL_NET_DEFAULT_PORT);
            udp_recv(udp_client_pcb, net_client_udp_recv_callback, NULL);
        }
        cyw43_arch_lwip_end();

        // Connect asynchronously without blocking main thread
        int err = cyw43_arch_wifi_connect_async(ssid, get_current_password(), CYW43_AUTH_WPA2_AES_PSK);
        if (err == 0) {
            current_status = NET_STATUS_CONNECTING;
        } else {
            current_status = NET_STATUS_ERROR;
        }
    }
#else
    current_status = NET_STATUS_DISCONNECTED;
#endif
}

static uint32_t last_reconnect_attempt_ms = 0;

void net_client_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_status_check_ms < 500) {
        return;
    }
    last_status_check_ms = now;
    cyw43_arch_lwip_begin();
    bool stalled = resp_is_receiving && (now - last_resp_chunk_rx_ms > 10000);
    cyw43_arch_lwip_end();
    if (stalled) { net_client_cancel(); response_failed = true; }

    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
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
                udp_bind(udp_client_pcb, IP_ADDR_ANY, CELESTIAL_NET_DEFAULT_PORT);
                udp_recv(udp_client_pcb, net_client_udp_recv_callback, NULL);
            }
            cyw43_arch_lwip_end();
        }
        struct netif *nif = &cyw43_state.netif[CYW43_ITF_STA];
        ip4addr_ntoa_r(netif_ip4_addr(nif), current_ip_str, sizeof(current_ip_str));

        // Periodic heartbeat ping to verify Brain Server is alive (every 1.0 s)
        if (now - last_heartbeat_tx_ms >= 1000) {
            last_heartbeat_tx_ms = now;
            net_client_ping_server();
        }

    } else {
        if (link_status == CYW43_LINK_BADAUTH || link_status == CYW43_LINK_NONET) {
            current_status = NET_STATUS_ERROR;
        } else {
            current_status = NET_STATUS_CONNECTING;
        }

#if HAVE_WIFI_CREDENTIALS
        if (now - last_reconnect_attempt_ms > 4000) {
            last_reconnect_attempt_ms = now;
            ap_connect_failures++;
            if (strlen(WIFI_FALLBACK_SSID) > 0 && ap_connect_failures >= 3) {
                // Toggle between primary (0) and fallback (1)
                active_ap_profile = (active_ap_profile == 0) ? 1 : 0;
                ap_connect_failures = 0;
                active_server_port = get_current_server_port();
                ipaddr_aton(get_current_server_ip_str(), &server_ip_addr);
                printf("[NET] Failing over to AP profile %d: [%s] (Target Server: %s:%d)\n",
                       active_ap_profile, get_current_ssid(), get_current_server_ip_str(), (int)active_server_port);
            }
            const char *ssid = get_current_ssid();
            if (strlen(ssid) > 0) {
                cyw43_arch_wifi_connect_async(ssid, get_current_password(), CYW43_AUTH_WPA2_AES_PSK);
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
    return (last_server_heartbeat_rx_ms > 0 && (now - last_server_heartbeat_rx_ms <= 8000));
}

const char* net_client_get_ip_str(void) {
    return current_ip_str;
}

const char* net_client_get_active_ssid(void) {
    return get_current_ssid();
}

const char* net_client_get_server_ip_str(void) {
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

    err_t err = udp_sendto(udp_client_pcb, p, &server_ip_addr, active_server_port);
    pbuf_free(p);

    cyw43_arch_lwip_end();

    return (err == ERR_OK);
}

bool net_client_ping_server(void) {
    // 1. Unicast ping to Brain Server
    send_udp_packet(NET_MSG_HEARTBEAT, NULL, 0);

    // 2. Broadcast Discovery Beacon to subnet (255.255.255.255:5005) so Companion App discovers staff
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
    audio_stop_all();
    resp_is_receiving = false;
    resp_is_ready = false;
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
    if (!pending_remote_cmd.has_command) {
        return false;
    }
    *cmd = pending_remote_cmd;
    pending_remote_cmd.has_command = false;
    return true;
}

bool net_client_has_paired_companion(void) {
    return has_paired_companion;
}

void net_client_send_telemetry(uint8_t mode, uint8_t state, uint8_t volume, uint8_t anim, uint8_t clock_style) {
    if (!has_paired_companion || udp_client_pcb == NULL || current_status != NET_STATUS_CONNECTED) {
        return;
    }
    uint8_t payload[6] = { mode, state, volume, anim, clock_style, 0 };
    size_t total_len = sizeof(net_packet_header_t) + sizeof(payload);
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
    if (p != NULL) {
        net_packet_header_t *hdr = (net_packet_header_t *)p->payload;
        hdr->magic = CELESTIAL_NET_MAGIC;
        hdr->version = CELESTIAL_NET_VERSION;
        hdr->msg_type = NET_MSG_APP_TELEMETRY;
        hdr->seq_num = tx_seq_counter++;
        hdr->payload_len = sizeof(payload);
        hdr->request_id = 0;
        memcpy((uint8_t *)p->payload + sizeof(*hdr), payload, sizeof(payload));
        udp_sendto(udp_client_pcb, p, &companion_ip_addr, companion_port);
        pbuf_free(p);
    }
    cyw43_arch_lwip_end();
}
