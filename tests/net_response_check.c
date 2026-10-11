#include <assert.h>
#include <stdio.h>
#include "../firmware/src/response_gate.h"

int main(void) {
    response_gate_t g = {0};
    response_gate_begin(&g, 42);
    assert(!response_gate_start(&g, 41));
    assert(response_gate_start(&g, 42));
    assert(response_gate_chunk(&g, 42, 0) == 2);
    assert(response_gate_start(&g, 42)); /* redundant START must not rewind */
    assert(g.next_seq == 1);
    assert(response_gate_chunk(&g, 42, 0) == 1); /* retransmit, not more sound */
    assert(response_gate_chunk(&g, 42, 2) == 0); /* gap: retry missing packet */
    assert(!response_gate_end(&g, 42, 2));
    assert(response_gate_chunk(&g, 42, 1) == 2);
    assert(response_gate_end(&g, 42, 2));
    assert(response_gate_end(&g, 42, 2)); /* lost END ACK */
    assert(response_gate_chunk(&g, 42, 2) == 0);
    response_gate_cancel(&g);
    assert(!response_gate_start(&g, 42)); /* late generation after timeout */
    response_gate_begin(&g, 43);
    assert(!response_gate_start(&g, 42)); /* old worker/new query isolation */
    assert(response_gate_start(&g, 43));
    puts("Response gate: duplicate START/chunks, gaps, END and stale replies PASS");
    return 0;
}
