import wave
import struct
import math
import os

TARGET_RATE = 16000

def resample_and_convert(wav_path):
    with wave.open(wav_path, 'rb') as w:
        nchannels = w.getnchannels()
        sampwidth = w.getsampwidth()
        framerate = w.getframerate()
        nframes = w.getnframes()
        raw_data = w.readframes(nframes)
    
    # Unpack 16-bit signed PCM
    samples = []
    if sampwidth == 2:
        fmt = f"<{nframes * nchannels}h"
        unpacked = struct.unpack(fmt, raw_data)
        if nchannels == 2:
            samples = [(unpacked[i*2] + unpacked[i*2+1]) // 2 for i in range(nframes)]
        else:
            samples = list(unpacked)
    elif sampwidth == 1:
        fmt = f"<{nframes * nchannels}B"
        unpacked = struct.unpack(fmt, raw_data)
        samples = [(x - 128) * 256 for x in unpacked]
    
    # Simple linear interpolation resampling to TARGET_RATE
    duration = nframes / framerate
    target_len = int(duration * TARGET_RATE)
    resampled = []
    for i in range(target_len):
        src_pos = i * (framerate / TARGET_RATE)
        idx = int(src_pos)
        frac = src_pos - idx
        if idx + 1 < len(samples):
            val = samples[idx] * (1.0 - frac) + samples[idx+1] * frac
        elif idx < len(samples):
            val = float(samples[idx])
        else:
            val = 0.0
        resampled.append(val)
    
    # Trim leading and trailing silence (threshold = 300)
    start_idx = 0
    while start_idx < len(resampled) and abs(resampled[start_idx]) < 300:
        start_idx += 1
    end_idx = len(resampled) - 1
    while end_idx > start_idx and abs(resampled[end_idx]) < 300:
        end_idx -= 1
    
    trimmed = resampled[start_idx:end_idx+1]
    if not trimmed:
        trimmed = [0]
    
    # Peak normalize to 8-bit signed range (-120 to +120)
    max_val = max(abs(x) for x in trimmed) if trimmed else 1
    scale = 120.0 / max(max_val, 1)
    
    # Soft robotic ring-mod effect / slight robotic tint (frequency modulation) or pure clean voice
    # Let's keep it very clean and natural with smooth fade-in/fade-out
    pcm8 = []
    fade_len = min(160, len(trimmed)) # 10 ms ramp
    for i, s in enumerate(trimmed):
        env = 1.0
        if i < fade_len:
            env = i / fade_len
        elif i > len(trimmed) - fade_len:
            env = (len(trimmed) - i) / fade_len
        val = int(s * scale * env)
        val = max(-127, min(127, val))
        pcm8.append(val)
        
    return pcm8

phrases = [
    ("VOICE_ONLINE", "tools/engine_online.wav", "CELESTIAL ONLINE"),
    ("VOICE_READY", "tools/system_ready.wav", "SYSTEM READY"),
    ("VOICE_ORACLE", "tools/oracle_active.wav", "ORACLE ACTIVATED"),
    ("VOICE_WARNING", "tools/warning_anomaly.wav", "ANOMALY DETECTED"),
]

data_dict = {}
for code, path, desc in phrases:
    pcm = resample_and_convert(path)
    data_dict[code] = (desc, pcm)
    print(f"Phrase {code} ({desc}): {len(pcm)} samples ({len(pcm)/16000:.2f} s, {len(pcm)/1024:.1f} KB)")

# Generate voice_data.h
h_content = """#ifndef CELESTIAL_VOICE_DATA_H
#define CELESTIAL_VOICE_DATA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
"""
for code, path, desc in phrases:
    h_content += f"    {code},\n"
h_content += """    VOICE_PHRASE_COUNT
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
"""

with open("firmware/src/voice_data.h", "w", encoding="utf-8") as f:
    f.write(h_content)

# Generate voice_data.c
c_content = """#include "voice_data.h"

"""

for code, path, desc in phrases:
    samples = data_dict[code][1]
    c_content += f"static const int8_t {code}_DATA[{len(samples)}] = {{\n"
    # Format 20 numbers per line
    for i in range(0, len(samples), 20):
        chunk = samples[i:i+20]
        c_content += "    " + ", ".join(f"{x}" for x in chunk) + ",\n"
    c_content += "};\n\n"

c_content += "static const voice_clip_t VOICE_CLIPS[VOICE_PHRASE_COUNT] = {\n"
for code, path, desc in phrases:
    samples = data_dict[code][1]
    c_content += f'    [ {code} ] = {{ "{desc}", {code}_DATA, {len(samples)} }},\n'
c_content += """};

const voice_clip_t* voice_get_clip(voice_phrase_id_t id) {
    if (id < VOICE_PHRASE_COUNT) {
        return &VOICE_CLIPS[id];
    }
    return NULL;
}
"""

with open("firmware/src/voice_data.c", "w", encoding="utf-8") as f:
    f.write(c_content)

print("firmware/src/voice_data.h and voice_data.c successfully generated!")
