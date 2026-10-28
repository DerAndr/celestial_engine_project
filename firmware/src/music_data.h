#ifndef CELESTIAL_MUSIC_DATA_H
#define CELESTIAL_MUSIC_DATA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MUSIC_TRACK_GHOSTBUSTERS = 0,
    MUSIC_TRACK_ADDAMS_FAMILY,
    MUSIC_TRACK_DAFT_PUNK,
    MUSIC_ADPCM_TRACK_COUNT
} music_adpcm_id_t;

typedef struct {
    const char *name;
    const uint8_t *adpcm_data;
    size_t sample_count; // total 16-bit audio samples (each byte = 2 samples)
    size_t byte_count;
} music_adpcm_clip_t;

const music_adpcm_clip_t* music_get_adpcm_clip(music_adpcm_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_MUSIC_DATA_H */
