#ifndef CELESTIAL_VOICE_DATA_H
#define CELESTIAL_VOICE_DATA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VOICE_ONLINE,
    VOICE_READY,
    VOICE_ORACLE,
    VOICE_WARNING,
    VOICE_PHRASE_COUNT
} voice_phrase_id_t;

typedef struct {
    const char *name;
    const int8_t *samples;
    size_t sample_count;
} voice_clip_t;

const voice_clip_t* voice_get_clip(voice_phrase_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_VOICE_DATA_H */
