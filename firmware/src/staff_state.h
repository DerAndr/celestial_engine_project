#ifndef CELESTIAL_STAFF_STATE_H
#define CELESTIAL_STAFF_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_BOOT = 0,
    STATE_CONNECTING,
    STATE_IDLE,
    STATE_LISTENING,
    STATE_UPLOADING,
    STATE_PROCESSING,
    STATE_SPEAKING,
    STATE_OFFLINE,
    STATE_ERROR,
    STATE_COUNT
} staff_state_t;

static inline const char *staff_state_name(staff_state_t state) {
    switch (state) {
        case STATE_BOOT:        return "BOOT";
        case STATE_CONNECTING:  return "CONNECTING";
        case STATE_IDLE:        return "IDLE";
        case STATE_LISTENING:   return "LISTENING";
        case STATE_UPLOADING:   return "UPLOADING";
        case STATE_PROCESSING:  return "PROCESSING";
        case STATE_SPEAKING:    return "SPEAKING";
        case STATE_OFFLINE:     return "OFFLINE";
        case STATE_ERROR:       return "ERROR";
        default:                return "UNKNOWN";
    }
}

#endif /* CELESTIAL_STAFF_STATE_H */
