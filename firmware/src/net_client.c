#include "net_client.h"
#include "net_protocol.h"
#include "audio.h"
#include "pico/stdlib.h"
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

#define RESP_BUFFER_MAX_SAMPLES 16000 // 1.0 second backward compatibility preview = 32 KB

static net_status_t current_status = NET_STATUS_DISCONNECTED;
static char current_ip_str[24] = "0.0.0.0";
static struct udp_pcb *udp_client_pcb = NULL;
static ip_addr_t server_ip_addr;
static uint16_t tx_seq_counter = 0;
static uint32_t last_status_check_ms = 0;

// Response buffer & streaming state
static int16_t resp_audio_buffer[RESP_BUFFER_MAX_SAMPLES];
static volatile size_t resp_sample_count = 0;
static volatile bool resp_is_receiving = false;
static volatile bool resp_is_ready = false;
static char resp_text_buffer[512] = "";

static uint32_t last_server_heartbeat_rx_ms = 0;
static uint32_t last_heartbeat_tx_ms = 0;

static uint32_t last_resp_chunk_rx_ms = 0;

static void net_client_udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                         const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)pcb;
    (void)addr;
    (void)port;

    if (p == NULL) {
        return;
    }

    if (p->tot_len >= sizeof(net_packet_header_t)) {
        net_packet_header_t hdr;
        pbuf_copy_partial(p, &hdr, sizeof(hdr), 0);

        if (hdr.magic == CELESTIAL_NET_MAGIC && hdr.version == CELESTIAL_NET_VERSION) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (hdr.msg_type == NET_MSG_HEARTBEAT) {
                last_server_heartbeat_rx_ms = now;
            } else if (hdr.msg_type == NET_MSG_RESP_TEXT) {
                size_t len = hdr.payload_len;
                if (len >= sizeof(resp_text_buffer)) {
                    len = sizeof(resp_text_buffer) - 1;
                }
                if (len > 0) {
                    pbuf_copy_partial(p, resp_text_buffer, len, sizeof(hdr));
                }
                resp_text_buffer[len] = '\0';
            } else if (hdr.msg_type == NET_MSG_RESP_START) {
                audio_stream_reset();
                resp_sample_count = 0;
                resp_is_receiving = true;
                resp_is_ready = false;
                last_resp_chunk_rx_ms = now;
            } else if (hdr.msg_type == NET_MSG_RESP_CHUNK) {
                // If START packet was dropped on Wi-Fi, auto-start receiving!
                if (!resp_is_receiving && !resp_is_ready) {
                    audio_stream_reset();
                    resp_sample_count = 0;
                    resp_is_receiving = true;
                }
                if (resp_is_receiving && hdr.payload_len > 0) {
                    size_t samples_in_chunk = hdr.payload_len / sizeof(int16_t);
                    int16_t chunk_buf[NET_MAX_PAYLOAD_SIZE / sizeof(int16_t)];
                    pbuf_copy_partial(p, chunk_buf, hdr.payload_len, sizeof(hdr));

                    // Push to infinite streaming ring buffer
                    audio_stream_push(chunk_buf, samples_in_chunk);

                    // Maintain backward-compatible buffer copy if within capacity
                    if (resp_sample_count + samples_in_chunk <= RESP_BUFFER_MAX_SAMPLES) {
                        memcpy(&resp_audio_buffer[resp_sample_count], chunk_buf, hdr.payload_len);
                        resp_sample_count += samples_in_chunk;
                    }
                    last_resp_chunk_rx_ms = now;
                }
            } else if (hdr.msg_type == NET_MSG_RESP_END) {
                if (resp_is_receiving) {
                    audio_stream_finish();
                    resp_is_receiving = false;
                    resp_is_ready = true;
                }
            }
        }
    }

    pbuf_free(p);
}

void net_client_init(void) {
    ipaddr_aton(BRAIN_SERVER_IP, &server_ip_addr);

#if HAVE_WIFI_CREDENTIALS
    if (strlen(WIFI_SSID) > 0) {
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
            udp_recv(udp_client_pcb, net_client_udp_recv_callback, NULL);
        }
        cyw43_arch_lwip_end();

        // Connect asynchronously without blocking main thread
        int err = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
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

    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link_status == CYW43_LINK_UP) {
        if (current_status != NET_STATUS_CONNECTED) {
            current_status = NET_STATUS_CONNECTED;
        }
        if (udp_client_pcb == NULL) {
            cyw43_arch_lwip_begin();
            udp_client_pcb = udp_new();
            if (udp_client_pcb != NULL) {
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

        // Auto-finalize if response chunks stopped arriving for 1500 ms (lost END packet safety net)
        if (resp_is_receiving && (now - last_resp_chunk_rx_ms > 1500)) {
            audio_stream_finish();
            resp_is_receiving = false;
            resp_is_ready = true;
        }
    } else {
        if (link_status == CYW43_LINK_BADAUTH || link_status == CYW43_LINK_NONET) {
            current_status = NET_STATUS_ERROR;
        } else {
            current_status = NET_STATUS_CONNECTING;
        }

#if HAVE_WIFI_CREDENTIALS
        if (strlen(WIFI_SSID) > 0 && (now - last_reconnect_attempt_ms > 4000)) {
            last_reconnect_attempt_ms = now;
            cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
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

    if (payload != NULL && payload_len > 0) {
        memcpy((uint8_t *)p->payload + sizeof(net_packet_header_t), payload, payload_len);
    }

    err_t err = udp_sendto(udp_client_pcb, p, &server_ip_addr, BRAIN_SERVER_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();

    return (err == ERR_OK);
}

bool net_client_ping_server(void) {
    return send_udp_packet(NET_MSG_HEARTBEAT, NULL, 0);
}

bool net_client_send_query_start(uint8_t lang_code, uint8_t model_preset) {
    tx_seq_counter = 0;
    resp_is_ready = false;
    resp_is_receiving = false;
    resp_sample_count = 0;
    resp_text_buffer[0] = '\0';
    last_resp_chunk_rx_ms = to_ms_since_boot(get_absolute_time());
    uint8_t payload[2] = { lang_code, model_preset };
    return send_udp_packet(NET_MSG_QUERY_START, payload, sizeof(payload));
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
    return send_udp_packet(NET_MSG_QUERY_END, NULL, 0);
}

void net_client_cancel(void) {
    send_udp_packet(NET_MSG_CANCEL, NULL, 0);
    audio_stream_reset();
    resp_is_receiving = false;
    resp_is_ready = false;
}

bool net_client_is_receiving_response(void) {
    return resp_is_receiving;
}

bool net_client_has_response_ready(void) {
    return resp_is_ready;
}

const int16_t* net_client_get_response_samples(size_t *out_sample_count) {
    if (out_sample_count != NULL) {
        *out_sample_count = resp_sample_count;
    }
    return resp_audio_buffer;
}

void net_client_consume_response(void) {
    resp_is_ready = false;
    resp_is_receiving = false;
    resp_sample_count = 0;
}

const char* net_client_get_response_text(void) {
    return resp_text_buffer;
}
