#include "adpcm_codec.h"

static const int32_t IMA_STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t IMA_INDEX_TABLE[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static inline int32_t clamp_s16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return v;
}

static inline int32_t clamp_idx(int32_t idx) {
    if (idx < 0) return 0;
    if (idx > 88) return 88;
    return idx;
}

static uint8_t encode_nibble(int32_t sample, int32_t *predictor, int32_t *step_index) {
    int32_t step = IMA_STEP_TABLE[*step_index];
    int32_t diff = sample - *predictor;
    uint8_t nibble = 0;

    if (diff < 0) {
        nibble = 8;
        diff = -diff;
    }

    int32_t delta = step >> 3;
    if (diff >= step) {
        nibble |= 4;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if (diff >= step) {
        nibble |= 2;
        diff -= step;
        delta += step;
    }
    step >>= 1;
    if (diff >= step) {
        nibble |= 1;
        delta += step;
    }

    if (nibble & 8) {
        *predictor = clamp_s16(*predictor - delta);
    } else {
        *predictor = clamp_s16(*predictor + delta);
    }

    *step_index = clamp_idx(*step_index + IMA_INDEX_TABLE[nibble]);
    return nibble;
}

static int16_t decode_nibble(uint8_t nibble, int32_t *predictor, int32_t *step_index) {
    int32_t step = IMA_STEP_TABLE[*step_index];
    int32_t delta = step >> 3;
    if (nibble & 4) delta += step;
    if (nibble & 2) delta += (step >> 1);
    if (nibble & 1) delta += (step >> 2);

    if (nibble & 8) {
        *predictor = clamp_s16(*predictor - delta);
    } else {
        *predictor = clamp_s16(*predictor + delta);
    }

    *step_index = clamp_idx(*step_index + IMA_INDEX_TABLE[nibble & 0x0F]);
    return (int16_t)(*predictor);
}

size_t ima_adpcm_encode_block(const int16_t *pcm_in, size_t num_samples, uint8_t *out_buf, size_t out_cap) {
    if (!pcm_in || !out_buf || num_samples == 0) return 0;
    size_t payload_bytes = (num_samples + 1u) / 2u;
    size_t total_bytes = IMA_ADPCM_HEADER_BYTES + payload_bytes;
    if (out_cap < total_bytes) return 0;

    int32_t predictor = pcm_in[0];
    int32_t step_index = 0;

    out_buf[0] = (uint8_t)(predictor & 0xFF);
    out_buf[1] = (uint8_t)((predictor >> 8) & 0xFF);
    out_buf[2] = (uint8_t)(step_index & 0xFF);
    out_buf[3] = 0;

    size_t out_pos = IMA_ADPCM_HEADER_BYTES;
    for (size_t i = 0; i < num_samples; i += 2u) {
        uint8_t lo = encode_nibble(pcm_in[i], &predictor, &step_index);
        uint8_t hi = (i + 1u < num_samples) ? encode_nibble(pcm_in[i + 1u], &predictor, &step_index) : 0u;
        out_buf[out_pos++] = (uint8_t)((lo & 0x0Fu) | ((hi & 0x0Fu) << 4));
    }
    return total_bytes;
}

size_t ima_adpcm_decode_block(const uint8_t *adpcm_in, size_t adpcm_len, int16_t *pcm_out, size_t max_samples) {
    if (!adpcm_in || !pcm_out || adpcm_len <= IMA_ADPCM_HEADER_BYTES || max_samples == 0) return 0;

    int32_t predictor = (int16_t)((uint16_t)adpcm_in[0] | ((uint16_t)adpcm_in[1] << 8));
    int32_t step_index = clamp_idx((int32_t)adpcm_in[2]);

    size_t out_count = 0;
    for (size_t pos = IMA_ADPCM_HEADER_BYTES; pos < adpcm_len && out_count < max_samples; ++pos) {
        uint8_t byte_val = adpcm_in[pos];
        pcm_out[out_count++] = decode_nibble(byte_val & 0x0Fu, &predictor, &step_index);
        if (out_count < max_samples) {
            pcm_out[out_count++] = decode_nibble((byte_val >> 4) & 0x0Fu, &predictor, &step_index);
        }
    }
    return out_count;
}
