#ifndef CELESTIAL_RESPONSE_GATE_H
#define CELESTIAL_RESPONSE_GATE_H
#include <stdbool.h>
#include <stdint.h>

/* Wire order belongs to one live request. ACK duplicates without replaying them. */
typedef struct {
    uint32_t request_id;
    uint16_t next_seq;
    bool active, started, ended;
} response_gate_t;
static inline void response_gate_begin(response_gate_t *g, uint32_t id) {
    *g = (response_gate_t){ .request_id = id, .active = true };
}
static inline void response_gate_cancel(response_gate_t *g) { g->active = false; }
static inline bool response_gate_matches(const response_gate_t *g, uint32_t id) {
    return g->active && g->request_id == id;
}
static inline bool response_gate_start(response_gate_t *g, uint32_t id) {
    if (!response_gate_matches(g, id) || g->ended) return false;
    g->started = true;
    return true;
}
/* 0: reject; 1: already accepted retransmit; 2: next chunk (commit after push). */
static inline int response_gate_chunk(response_gate_t *g, uint32_t id, uint16_t seq) {
    if (!response_gate_matches(g, id) || !g->started || g->ended) return 0;
    if (seq == g->next_seq) { g->next_seq++; return 2; }
    if (g->next_seq && seq == (uint16_t)(g->next_seq - 1)) return 1;
    return 0;
}
static inline bool response_gate_end(response_gate_t *g, uint32_t id, uint16_t seq) {
    if (!response_gate_matches(g, id) || !g->started || seq != g->next_seq) return false;
    g->ended = true;
    return true;
}
#endif
