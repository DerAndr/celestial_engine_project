#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_STATUS_DISCONNECTED = 0,
    NET_STATUS_CONNECTING,
    NET_STATUS_CONNECTED,
    NET_STATUS_ERROR
} net_status_t;

// Initialize UDP networking & initiate background Wi-Fi connection
void net_client_init(void);

// Call periodically from main loop (e.g. every frame)
void net_client_update(void);

// Connection status
net_status_t net_client_get_status(void);
bool net_client_is_connected(void);
bool net_client_is_server_ready(void);
bool net_client_ping_server(void);
const char* net_client_get_ip_str(void);
const char* net_client_get_active_ssid(void);
const char* net_client_get_server_ip_str(void);
uint16_t net_client_get_server_port(void);


// Dynamic AI Host & Model Catalog
net_host_type_t net_client_get_host_type(void);
const char* net_client_get_host_name(void);
void net_client_set_preferred_host(net_host_type_t pref);
net_host_type_t net_client_get_preferred_host(void);
uint8_t net_client_get_model_count(void);
const char* net_client_get_model_name(uint8_t idx);
const char* net_client_get_model_tag(uint8_t idx);
uint8_t net_client_get_model_theme(uint8_t idx);
uint8_t net_client_get_model_id(uint8_t idx);
bool net_client_request_catalog(void);

// Voice query streaming to Brain Server (passes selected language and model preset)
bool net_client_send_query_start(uint8_t lang_code, uint8_t model_preset);
bool net_client_send_audio_chunk(const int16_t *samples, size_t sample_count);
bool net_client_send_query_end(void);
void net_client_cancel(void);
bool net_client_check_and_clear_cancel(void);

// Response state from Brain Server
bool net_client_is_receiving_response(void);
bool net_client_has_response_ready(void);
void net_client_consume_response(void);
const char* net_client_get_response_text(void);
bool net_client_response_failed(void);
uint32_t net_client_response_progress_ms(void);

// Companion App Remote Control (Authenticated by CELESTIAL_AUTH_SECRET)
typedef struct {
    uint8_t cmd_code;       // 'M'=mode, 'V'=volume, 'L'=LED, 'K'=clock, 'C'=sfx, 'T'=track, 'P'=phrase, 'S'=stop, 'X'=time sync
    uint8_t param;
    uint32_t val32;
    uint32_t val32_extra;
    bool has_command;
} remote_cmd_t;

bool net_client_poll_remote_command(remote_cmd_t *cmd);
void net_client_send_telemetry(uint8_t mode, uint8_t state, uint8_t volume, uint8_t anim, uint8_t clock_style,
                               uint8_t lang, uint8_t host_type, uint8_t pref_host,
                               uint8_t ai_model, uint8_t mic_level, uint8_t cosmos_zoom, uint8_t is_audio_playing);
bool net_client_has_paired_companion(void);
bool net_client_has_unsolicited_text(void);
void net_client_clear_unsolicited_text(void);

#ifdef __cplusplus
}
#endif

#endif // NET_CLIENT_H
