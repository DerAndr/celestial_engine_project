#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Celestial Engine Mk.V Network Protocol Definition
// Minimal, low-latency UDP binary protocol for voice streaming between Pico 2 W and Brain Server.

#define CELESTIAL_NET_MAGIC         0x454C4543  // 'C' 'E' 'L' 'E' in Little-Endian
#define CELESTIAL_NET_VERSION       2
#define CELESTIAL_NET_DEFAULT_PORT  5005

typedef enum {
    NET_MSG_HEARTBEAT    = 0x01,  // Periodic ping/keepalive
    NET_MSG_QUERY_START  = 0x02,  // User pressed QUERY; speech begins
    NET_MSG_AUDIO_CHUNK  = 0x03,  // 16 kHz 16-bit mono PCM audio data chunk
    NET_MSG_QUERY_END    = 0x04,  // User released QUERY; speech complete
    NET_MSG_RESP_START   = 0x05,  // Server begins sending voice response
    NET_MSG_RESP_CHUNK   = 0x06,  // Server audio data chunk for playback
    NET_MSG_RESP_END     = 0x07,  // Server finished response
    NET_MSG_CANCEL       = 0x08,  // Abort current transaction
    NET_MSG_RESP_TEXT    = 0x09,  // Server sends ASCII response transcript
    NET_MSG_ACK          = 0x0A,  // payload: acknowledged message type (one byte)
    NET_MSG_PROGRESS     = 0x0B,  // Server is still generating this request
    NET_MSG_ERROR        = 0x0C   // Explicit failure; never play a truncated success
} net_msg_type_t;

typedef enum {
    LANG_EN   = 0,  // English (Default)
    LANG_AUTO = 1,  // Auto-detect on server
    LANG_DE   = 2,  // Deutsch
    LANG_RU   = 3,  // Russian
    LANG_R2D2 = 4,  // Astromech Droid [R2]
    LANG_COUNT
} staff_lang_id_t;

typedef enum {
    AI_MODEL_DEEPSEEK = 0,  // deepseek/deepseek-chat (Deep Thought — fastest & cheapest)
    AI_MODEL_MISTRAL  = 1,  // mistralai/ministral-8b-2512 (Eddie the Ship Computer)
    AI_MODEL_GROK     = 2,  // x-ai/grok-4.5 (Grok 4.5 Hitchhiker)
    AI_MODEL_LLAMA    = 3,  // meta-llama/llama-3.3-70b-instruct (Marvin 70B)
    AI_MODEL_COUNT
} staff_ai_model_id_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         // CELESTIAL_NET_MAGIC
    uint8_t  version;       // CELESTIAL_NET_VERSION
    uint8_t  msg_type;      // net_msg_type_t
    uint16_t seq_num;       // Monotonically increasing sequence number
    uint16_t payload_len;   // Byte count of payload following this header
    uint32_t request_id;    // One transaction; heartbeat uses zero
} net_packet_header_t;
#pragma pack(pop)
_Static_assert(sizeof(net_packet_header_t) == 14, "Protocol v2 header must match Python <IBBHHI");

#define NET_MAX_PAYLOAD_SIZE 512  // 256 samples (16 ms) of 16-bit PCM = 512 bytes

#ifdef __cplusplus
}
#endif

#endif // NET_PROTOCOL_H
