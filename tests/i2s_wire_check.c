/* Decode the actual pioasm output as a Philips-I2S receiver would see it.
 * Deliberately independent of audio.c's PCM generator and hardware stubs. */
#include <stdint.h>
#include <stdio.h>
#include "audio_wire_program.h"

static int decode_program(const uint16_t *program, unsigned length, int msb_first) {
    const int16_t samples[] = {0, 256, -256, 512, -512, 0x1234, -0x1234, 0x7fff, (int16_t)0x8000u, 4096, -4096, 0};
    const unsigned frames = sizeof samples / sizeof samples[0];
    unsigned pc = 0, x = 0, shifted = 32, word_index = 0, old_clock = 0;
    unsigned lr = 0, receiver_lr = 0, bits = 0, decoded = 0, clocks = 0;
    uint32_t osr = 0, received = 0;
    unsigned data = 0;
    int synchronized = 0;
    for (unsigned tick = 0; tick < frames * 128; ++tick) {
        uint16_t ins = program[pc];
        unsigned op = ins >> 13, next_pc = pc + 1;
        unsigned side = (ins >> 11) & 3;
        if (op == 7 && ((ins >> 5) & 7) == 1) x = ins & 31; // SET X
        else if (op == 3 && ((ins >> 5) & 7) == 0 && (ins & 31) == 1) {
            if (shifted == 32) {
                if (word_index >= frames) return 1;
                int16_t left = samples[word_index];
                int16_t right = samples[word_index];
                osr = ((uint32_t)(uint16_t)left << 16) | (uint32_t)(uint16_t)right;
                word_index++;
                shifted = 0;
            }
            data = msb_first ? osr >> 31 : osr & 1;
            osr = msb_first ? osr << 1 : osr >> 1;
            ++shifted;
        } else if (op == 0 && ((ins >> 5) & 7) == 2) {
            if (x-- != 0) next_pc = ins & 31;
        } else return 1;
        lr = side >> 1;
        unsigned clock = side & 1;
        if (!old_clock && clock) {
            ++clocks;
            // On the LRCLK transition edge, DIN still carries the previous
            // channel's LSB. The next rising edge carries the new MSB.
            if (synchronized) { received = (received << 1) | data; ++bits; }
            if (lr != receiver_lr) {
                if (synchronized) {
                    if (bits != 16) return 1;
                    unsigned expected_frame = (decoded + 1) / 2;
                    if (expected_frame >= frames || (int16_t)received != samples[expected_frame]) return 1;
                    ++decoded;
                }
                receiver_lr = lr;
                received = 0; bits = 0; synchronized = 1;
            }
        }
        old_clock = clock;
        pc = next_pc == length ? 0 : next_pc;
        if (pc >= length) return 1;
    }
    return decoded == frames * 2 - 2 && clocks == frames * 32 ? 0 : 1;
}

int main(void) {
    unsigned length = sizeof wire_program / sizeof wire_program[0];
    if (decode_program(wire_program, length, 1)) { puts("Actual PIO I2S signed-word decode: FAIL"); return 1; }
    puts("Actual pioasm program: 16 bits/channel, one-bit delay, MSB-first, signed L/R decode PASS");
    // Prove the receiver check rejects a plausible sign/bit-order defect.
    if (!decode_program(wire_program, length, 0)) { puts("Wrong shift-direction control was not detected"); return 1; }
    puts("Wrong shift-direction control: rejected PASS");
    return 0;
}
