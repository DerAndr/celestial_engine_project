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

// Voice query streaming to Brain Server (passes selected language and model preset)
bool net_client_send_query_start(uint8_t lang_code, uint8_t model_preset);
bool net_client_send_audio_chunk(const int16_t *samples, size_t sample_count);
bool net_client_send_query_end(void);
void net_client_cancel(void);

// Response state from Brain Server
bool net_client_is_receiving_response(void);
bool net_client_has_response_ready(void);
void net_client_consume_response(void);
const char* net_client_get_response_text(void);
bool net_client_response_failed(void);
uint32_t net_client_response_progress_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NET_CLIENT_H
