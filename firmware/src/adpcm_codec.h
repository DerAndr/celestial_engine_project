#ifndef ADPCM_CODEC_H
#define ADPCM_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMA_ADPCM_HEADER_BYTES 4

/**
 * Encodes `num_samples` of 16-bit signed mono PCM into a self-contained IMA-ADPCM block
 * with a 4-byte little-endian header: [int16_t initial_predictor][uint8_t step_index][uint8_t reserved=0].
 * Returns total bytes written to `out_buf` (4 + (num_samples + 1) / 2), or 0 on invalid arguments.
 */
size_t ima_adpcm_encode_block(const int16_t *pcm_in, size_t num_samples, uint8_t *out_buf, size_t out_cap);

/**
 * Decodes a self-contained IMA-ADPCM block (with 4-byte header) into 16-bit signed mono PCM.
 * Returns number of PCM samples written to `pcm_out` (up to `max_samples`), or 0 on invalid arguments.
 */
size_t ima_adpcm_decode_block(const uint8_t *adpcm_in, size_t adpcm_len, int16_t *pcm_out, size_t max_samples);

#ifdef __cplusplus
}
#endif

#endif /* ADPCM_CODEC_H */
