#include "leds.h"
#include "audio.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "pico/time.h"
#include "ws2812.pio.h"
#include <math.h>
#include <stdlib.h>

static PIO led_pio = pio0;
static uint led_sm = 1;
static uint led_offset = 0;

static uint8_t master_brightness = LED_BRIGHTNESS_LIMIT;
static absolute_time_t next_anim_time;
static uint32_t anim_tick = 0;
static int active_anim_mode = LED_ANIM_RAINBOW; // Full-array rainbow for instant visual verification
static int base_color_idx = 0;

// Individual pixel RGB buffer
static uint8_t pixels_r[LED_RING_COUNT];
static uint8_t pixels_g[LED_RING_COUNT];
static uint8_t pixels_b[LED_RING_COUNT];

static int led_dma_chan = -1;
static dma_channel_config led_dma_config;
static uint32_t led_tx_words[LED_RING_COUNT];
#if CELESTIAL_FOUR_RINGS
static absolute_time_t led_boot_test_started;
static int led_boot_test_phase = -1;
#endif

const led_color_t RAINBOW_COLORS[RAINBOW_COLOR_COUNT] = {
    { 255,   0,   0, "RED" },     // 0. Red
    { 255,  85,   0, "ORANGE" },  // 1. Orange
    { 255, 210,   0, "YELLOW" },  // 2. Yellow
    {   0, 255,   0, "GREEN" },   // 3. Green
    {   0, 200, 255, "CYAN" },    // 4. Cyan
    {   0,  30, 255, "BLUE" },    // 5. Blue
    { 180,   0, 255, "VIOLET" }   // 6. Violet
};

static const char *ANIM_NAMES[LED_ANIM_COUNT] = {
    "COSMIC BREATHE",
    "GALAXY VORTEX",
    "COSMIC SNAKE",
    "RAINBOW WAVE",
    "PULSAR PULSE",
    "SOLAR PLASMA",
    "STARRY NIGHT",
    "VOICE REACTIVE",
    "SOLAR ORRERY",
    "DATA RECEPTION",
    "SONIC EXPANSION"
};

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t r_scaled = (uint32_t)(r * master_brightness) / 100;
    uint32_t g_scaled = (uint32_t)(g * master_brightness) / 100;
    uint32_t b_scaled = (uint32_t)(b * master_brightness) / 100;
    return ((uint32_t)g_scaled << 16) | ((uint32_t)r_scaled << 8) | (uint32_t)b_scaled;
}

static void show_pixels(void) {
    // Finish packing the entire frame before starting PIO-paced DMA.
    // CPU/USB/Wi-Fi/audio interrupts must not interrupt feeding the pixel stream.
    for (int i = 0; i < LED_RING_COUNT; ++i) {
        led_tx_words[i] = rgb_to_grb(pixels_r[i], pixels_g[i], pixels_b[i]) << 8u;
    }
    __compiler_memory_barrier();
    dma_channel_configure(led_dma_chan, &led_dma_config,
                          &led_pio->txf[led_sm], led_tx_words, LED_RING_COUNT, true);
    dma_channel_wait_for_finish_blocking(led_dma_chan);

    // FIFO empty can still leave one 24-bit pixel in the output shifter.
    while (!pio_sm_is_tx_fifo_empty(led_pio, led_sm)) {
        tight_loop_contents();
    }
    sleep_us(35 + LED_RESET_US); // Finish the last 30us word, then a >=300us low latch.
}

// Simple HSV to RGB converter (h: 0..359, s: 0..255, v: 0..255)
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }
    uint8_t region = (h % 360) / 60;
    uint8_t remainder = ((h % 360) - (region * 60)) * 255 / 60;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

void leds_init(void) {
    led_offset = pio_add_program(led_pio, &ws2812_program);
    ws2812_program_init(led_pio, led_sm, led_offset, PIN_WS2812_DATA, 800000.0f, false);

    led_dma_chan = dma_claim_unused_channel(true);
    led_dma_config = dma_channel_get_default_config(led_dma_chan);
    channel_config_set_transfer_data_size(&led_dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&led_dma_config, true);
    channel_config_set_write_increment(&led_dma_config, false);
    channel_config_set_dreq(&led_dma_config, pio_get_dreq(led_pio, led_sm, true));
    channel_config_set_high_priority(&led_dma_config, true);

    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 0;
        pixels_g[i] = 0;
        pixels_b[i] = 0;
    }
    show_pixels();
    next_anim_time = get_absolute_time();
#if CELESTIAL_FOUR_RINGS
    led_boot_test_started = next_anim_time;
    led_boot_test_phase = -1;
#endif
}

void leds_set_pixel_rgb(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= 0 && index < LED_RING_COUNT) {
        pixels_r[index] = r;
        pixels_g[index] = g;
        pixels_b[index] = b;
    }
}

void leds_set_all_rgb(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = r;
        pixels_g[i] = g;
        pixels_b[i] = b;
    }
    show_pixels();
}

void leds_set_anim_mode(int mode) {
    if (mode >= 0 && mode < LED_ANIM_COUNT) {
        active_anim_mode = mode;
    }
}

int leds_get_anim_mode(void) {
    return active_anim_mode;
}

const char* leds_get_anim_name(int mode) {
    if (mode >= 0 && mode < LED_ANIM_COUNT) {
        return ANIM_NAMES[mode];
    }
    return "UNKNOWN";
}

void leds_set_rainbow_index(int color_idx) {
    if (color_idx < 0) color_idx = 0;
    base_color_idx = color_idx % RAINBOW_COLOR_COUNT;
}

const char* leds_get_rainbow_name(int color_idx) {
    if (color_idx < 0) color_idx = 0;
    return RAINBOW_COLORS[color_idx % RAINBOW_COLOR_COUNT].name;
}

void leds_set_brightness(uint8_t brightness_pct) {
    if (brightness_pct > LED_BRIGHTNESS_LIMIT) brightness_pct = LED_BRIGHTNESS_LIMIT;
    master_brightness = brightness_pct;
}

// -------------------------------------------------------------
// Core Animation Renderers (Optimized for 43 LEDs: 7 + 12 + 24)
// -------------------------------------------------------------

// 0. Slow cosmic breathing in active color across all 3 rings
static void render_breathe(uint32_t tick) {
    float angle = (float)(tick % 180) * 2.0f * 3.14159f / 180.0f;
    float breath = (sinf(angle) + 1.0f) * 0.5f; // 0.0 to 1.0
    float intensity = 0.2f + 0.8f * breath;

    uint8_t br = (uint8_t)(RAINBOW_COLORS[base_color_idx].r * intensity);
    uint8_t bg = (uint8_t)(RAINBOW_COLORS[base_color_idx].g * intensity);
    uint8_t bb = (uint8_t)(RAINBOW_COLORS[base_color_idx].b * intensity);

    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = br;
        pixels_g[i] = bg;
        pixels_b[i] = bb;
    }
}

// 1. Unified Logarithmic Whirlpool / Spiral across all rings simultaneously:
//    - Center (0): Swirling vortex core singularity (voice-reactive white/cyan flare)
//    - Ring 0 (1..6): Inner eye arms (radius 1.0)
//    - Ring 1 (7..18): Middle vortex arms (radius 2.0, curved 46 deg)
//    - Ring 2 (19..42): Outer galaxy spiral arms (radius 3.2, curved 95 deg)
//    - Ring 3 (43..58): Accretion spiral outer rim (radius 4.4, curved 143 deg)
static void render_vortex(uint32_t tick, uint8_t mic_level_pct) {
    float voice_gain = (float)mic_level_pct / 100.0f;
    if (voice_gain > 1.0f) voice_gain = 1.0f;

    // Center LED (index 0) - pulsating bright RED recording indicator (REC active!)
    float c_wave = (sinf((float)tick * 0.20f) + 1.0f) * 0.5f;
    pixels_r[0] = (uint8_t)(170 + 85.0f * (c_wave * 0.5f + voice_gain * 0.5f));
    pixels_g[0] = (uint8_t)(10.0f * voice_gain);
    pixels_b[0] = (uint8_t)(10.0f * voice_gain);

    // Angular rotation: accelerates dynamically when user speaks into mic!
    float rot_speed = 0.08f + 0.14f * voice_gain;
    float rot = (float)tick * rot_speed;

    // Two symmetric spiral arms: 180 degrees apart
    // Ring 0 (1..6, 6 LEDs, radius 1.0): twist = 0.0
    for (int i = 1; i <= 6; i++) {
        float theta = (float)(i - 1) * (2.0f * 3.14159265f / 6.0f);
        float diff = theta - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        pixels_r[i] = (uint8_t)(10  + pwr * (220.0f * voice_gain + 30.0f));
        pixels_g[i] = (uint8_t)(20  + pwr * 230.0f);
        pixels_b[i] = (uint8_t)(60  + pwr * 195.0f);
    }

    // Ring 1 (7..18, 12 LEDs, radius 2.0): twist = 0.80 rad (~46 deg)
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        float theta = (float)(i - 7) * (2.0f * 3.14159265f / 12.0f);
        float diff = theta - 0.80f - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        pixels_r[i] = (uint8_t)(20  + pwr * 180.0f);
        pixels_g[i] = (uint8_t)(15  + pwr * 210.0f);
        pixels_b[i] = (uint8_t)(50  + pwr * 205.0f);
    }

    // Ring 2 (19..42, 24 LEDs, radius 3.2): twist = 1.65 rad (~95 deg)
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        float theta = (float)(i - 19) * (2.0f * 3.14159265f / 24.0f);
        float diff = theta - 1.65f - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        pixels_r[i] = (uint8_t)(8   + pwr * 240.0f);
        pixels_g[i] = (uint8_t)(10  + pwr * 140.0f);
        pixels_b[i] = (uint8_t)(35  + pwr * 220.0f);
    }

#if CELESTIAL_FOUR_RINGS
    // Ring 3 (43..58, 16 LEDs, radius 4.4): twist = 2.50 rad (~143 deg)
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        float theta = (float)(i - 43) * (2.0f * 3.14159265f / 16.0f);
        float diff = theta - 2.50f - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        if (pwr >= 0.25f) {
            pixels_r[i] = (uint8_t)(pwr * 210.0f);
            pixels_g[i] = (uint8_t)(pwr * 60.0f);
            pixels_b[i] = (uint8_t)(pwr * 255.0f);
        } else {
            pixels_r[i] = 0; pixels_g[i] = 0; pixels_b[i] = 0; // Dark background
        }
    }
#endif
}

// 9. Concentric Data Reception: Rings pulse inward as distinct concentric circles
//    simulating sub-etha transmission packets cascading into the staff core!
static void render_data_recv(uint32_t tick) {
    uint32_t cycle = tick / 14;
    uint32_t phase = tick % 14;

    // Alternate packet beam colors: Sub-Etha Neon Cyan vs. Emerald Matrix telemetry
    bool alt_color = (cycle % 2) == 1;
    uint8_t pr = alt_color ? 0   : 0;
    uint8_t pg = alt_color ? 255 : 240;
    uint8_t pb = alt_color ? 130 : 255;

#if CELESTIAL_FOUR_RINGS
    float r3_int = (phase <= 2) ? (1.0f - (float)phase * 0.35f) : 0.0f;
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        if (r3_int > 0.05f) {
            pixels_r[i] = (uint8_t)(pr * r3_int);
            pixels_g[i] = (uint8_t)(pg * r3_int);
            pixels_b[i] = (uint8_t)(pb * r3_int);
        } else {
            pixels_r[i] = 0; pixels_g[i] = 0; pixels_b[i] = 0;
        }
    }
    float r2_int = (phase >= 2 && phase <= 5) ? (1.0f - (float)(phase - 2) * 0.30f) : 0.05f;
    float r1_int = (phase >= 5 && phase <= 8) ? (1.0f - (float)(phase - 5) * 0.30f) : 0.05f;
    float r0_int = (phase >= 8 && phase <= 11) ? (1.0f - (float)(phase - 8) * 0.30f) : 0.05f;
    float c_int  = (phase >= 11) ? (1.0f - (float)(phase - 11) * 0.40f) : 0.15f;
#else
    // 3 Concentric Rings: Ring 2 (outer) -> Ring 1 (middle) -> Ring 0 (inner) -> Center (core)
    float r2_int = (phase <= 3) ? (1.0f - (float)phase * 0.28f) : 0.04f;
    float r1_int = (phase >= 3 && phase <= 6) ? (1.0f - (float)(phase - 3) * 0.28f) : 0.04f;
    float r0_int = (phase >= 6 && phase <= 9) ? (1.0f - (float)(phase - 6) * 0.28f) : 0.04f;
    float c_int  = (phase >= 9 && phase <= 12) ? (1.0f - (float)(phase - 9) * 0.30f) : 0.10f;
#endif

    // Render Ring 2 as solid glowing concentric circle
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(pr * r2_int + 2);
        pixels_g[i] = (uint8_t)(pg * r2_int + 4);
        pixels_b[i] = (uint8_t)(pb * r2_int + 12);
    }

    // Render Ring 1 as solid glowing concentric circle
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(pr * r1_int + 2);
        pixels_g[i] = (uint8_t)(pg * r1_int + 4);
        pixels_b[i] = (uint8_t)(pb * r1_int + 12);
    }

    // Render Ring 0 as solid glowing concentric circle
    for (int i = 1; i <= 6; i++) {
        pixels_r[i] = (uint8_t)(pr * r0_int + 2);
        pixels_g[i] = (uint8_t)(pg * r0_int + 4);
        pixels_b[i] = (uint8_t)(pb * r0_int + 12);
    }

    // Center Core LED: Flashes pure diamond-white when packet hits core
    if (c_int > 0.4f) {
        pixels_r[0] = (uint8_t)(255 * c_int);
        pixels_g[0] = (uint8_t)(255 * c_int);
        pixels_b[0] = (uint8_t)(255 * c_int);
    } else {
        pixels_r[0] = (uint8_t)(20  + 60 * c_int);
        pixels_g[0] = (uint8_t)(100 + 80 * c_int);
        pixels_b[0] = (uint8_t)(160 + 95 * c_int);
    }
}

// 10. Expanding Speech Ripples: Sonic shockwaves radiating outward from center to outer perimeter
//     simulating acoustic oracle speech broadcast ("Listen! Oracle speaks!")
static void render_expand_speech(uint32_t tick) {
    // 16-tick wave cycle (~480 ms per ripple wave)
    uint32_t wave_phase = tick % 16;

    // Radiant celestial gold/amber voice wavefront
    uint8_t vr = 255;
    uint8_t vg = 180;
    uint8_t vb = 30;

    // Center Core (0): Fires brightly at phase 0..3, then gentle warm starlight
    float c_wave = (wave_phase <= 3) ? (1.0f - (float)wave_phase * 0.25f) : 0.20f;
    pixels_r[0] = (uint8_t)(255 * c_wave);
    pixels_g[0] = (uint8_t)(240 * c_wave);
    pixels_b[0] = (uint8_t)(100 * c_wave);

    // Ring 0 (1..6, radius 1.0): Peak at phase 2..6
    float r0_wave = 0.05f;
    if (wave_phase >= 2 && wave_phase <= 6) {
        float d = (float)(wave_phase - 2) / 4.0f;
        r0_wave = 1.0f - d * 0.8f;
    }
    for (int i = 1; i <= 6; i++) {
        pixels_r[i] = (uint8_t)(vr * r0_wave + 10);
        pixels_g[i] = (uint8_t)(vg * r0_wave + 5);
        pixels_b[i] = (uint8_t)(vb * r0_wave + 2);
    }

    // Ring 1 (7..18, radius 2.0): Peak at phase 5..9
    float r1_wave = 0.05f;
    if (wave_phase >= 5 && wave_phase <= 9) {
        float d = (float)(wave_phase - 5) / 4.0f;
        r1_wave = 1.0f - d * 0.8f;
    }
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(vr * r1_wave + 8);
        pixels_g[i] = (uint8_t)(vg * r1_wave + 4);
        pixels_b[i] = (uint8_t)(vb * r1_wave + 2);
    }

    // Ring 2 (19..42, radius 3.2): Peak at phase 8..13
    float r2_wave = 0.05f;
    if (wave_phase >= 8 && wave_phase <= 13) {
        float d = (float)(wave_phase - 8) / 5.0f;
        r2_wave = 1.0f - d * 0.8f;
    }
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(vr * r2_wave + 4);
        pixels_g[i] = (uint8_t)(vg * r2_wave + 2);
        pixels_b[i] = (uint8_t)(vb * r2_wave + 2);
    }

#if CELESTIAL_FOUR_RINGS
    // Ring 3 (43..58, radius 4.4): Outermost sonic horizon, peak at phase 11..15
    float r3_wave = 0.02f;
    if (wave_phase >= 11) {
        float d = (float)(wave_phase - 11) / 5.0f;
        r3_wave = 1.0f - d * 0.85f;
    }
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(vr * r3_wave);
        pixels_g[i] = (uint8_t)(vg * r3_wave);
        pixels_b[i] = (uint8_t)(vb * r3_wave);
    }
#endif
}

// 2. Cosmic Snake: Mythical dragon serpent slithering across all concentric rings (59 LEDs)
static void render_snake(uint32_t tick) {
    int count = LED_RING_COUNT;
    int head = tick % count;

    for (int i = 0; i < count; i++) {
        int dist = (head - i + count) % count;

        if (dist == 0) {
            // Snake Head: Brilliant gleaming diamond white-gold
            pixels_r[i] = 255;
            pixels_g[i] = 250;
            pixels_b[i] = 220;
        } else if (dist >= 1 && dist <= 14) {
            // Rainbow Serpent Body with smooth exponential fade
            float fade = 1.0f - ((float)dist / 15.0f);
            uint16_t hue = (tick * 6 + dist * 24) % 360;
            hsv_to_rgb(hue, 255, (uint8_t)(180 * fade), &pixels_r[i], &pixels_g[i], &pixels_b[i]);
        } else {
            // Resting cosmic starlight so all 4 rings remain subtly visible in the dark
            pixels_r[i] = 3;
            pixels_g[i] = 5;
            pixels_b[i] = 16;
        }
    }
}

// 3. Smooth Concentric Chromatic Rainbow Wave:
//    Radiating multi-ring spiral with coordinated angular phases across all 4 concentric rings
static void render_rainbow_wave(uint32_t tick) {
    uint16_t base_hue = (tick * 4) % 360;

    // Center Core LED (0)
    hsv_to_rgb(base_hue, 255, 180, &pixels_r[0], &pixels_g[0], &pixels_b[0]);

    // Ring 0 (1..6): 6 LEDs, 60-degree increments
    for (int i = 1; i <= 6 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + (i - 1) * 60) % 360;
        hsv_to_rgb(hue, 255, 180, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }

    // Ring 1 (7..18): 12 LEDs, 30-degree increments, shifted by 50 deg
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + 50 + (i - 7) * 30) % 360;
        hsv_to_rgb(hue, 255, 180, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }

    // Ring 2 (19..42): 24 LEDs, 15-degree increments, shifted by 100 deg
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + 100 + (i - 19) * 15) % 360;
        hsv_to_rgb(hue, 255, 180, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }

#if CELESTIAL_FOUR_RINGS
    // Ring 3 (43..58): 16 LEDs, 22.5-degree increments, shifted by 150 deg
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + 150 + (i - 43) * 22) % 360;
        hsv_to_rgb(hue, 255, 180, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }
#endif
}

// 4. Cosmic Heartbeat / Pulsar: 'ba-dum' shockwave expanding outward through all 4 rings
static void render_pulsar(uint32_t tick) {
    uint32_t phase = tick % 90; // Repeat every 3 seconds (30 Hz * 3)

    float c_val = 0.05f;
    float r0_val = 0.05f;
    float r1_val = 0.05f;
    float r3_val = 0.05f;
    float r2_val = 0.05f;

    // Pulse 1: 'ba' (expanding outward from center through 4 concentric layers)
    if (phase < 6) {
        c_val = 1.0f - ((float)phase / 6.0f);
    } else if (phase >= 4 && phase < 11) {
        r0_val = 1.0f - ((float)(phase - 4) / 7.0f);
    } else if (phase >= 8 && phase < 16) {
        r1_val = 1.0f - ((float)(phase - 8) / 8.0f);
    } else if (phase >= 11 && phase < 19) {
        r3_val = 1.0f - ((float)(phase - 11) / 8.0f);
    } else if (phase >= 14 && phase < 24) {
        r2_val = 1.0f - ((float)(phase - 14) / 10.0f);
    }

    // Pulse 2: 'dum' (larger shockwave)
    if (phase >= 20 && phase < 27) {
        c_val = 1.0f - ((float)(phase - 20) / 7.0f);
    } else if (phase >= 24 && phase < 32) {
        r0_val = 1.0f - ((float)(phase - 24) / 8.0f);
    } else if (phase >= 28 && phase < 38) {
        r1_val = 1.0f - ((float)(phase - 28) / 10.0f);
    } else if (phase >= 32 && phase < 42) {
        r3_val = 1.0f - ((float)(phase - 32) / 10.0f);
    } else if (phase >= 36 && phase < 48) {
        r2_val = 1.0f - ((float)(phase - 36) / 12.0f);
    }

    // Center LED (0): Brilliant white-cyan core
    pixels_r[0] = (uint8_t)(200 * c_val);
    pixels_g[0] = (uint8_t)(240 * c_val);
    pixels_b[0] = (uint8_t)(255 * c_val);

    // Ring 0 (1..6): Deep violet shockwave
    for (int i = 1; i <= 6; i++) {
        pixels_r[i] = (uint8_t)(240 * r0_val);
        pixels_g[i] = (uint8_t)(40  * r0_val);
        pixels_b[i] = (uint8_t)(255 * r0_val);
    }

    // Ring 1 (7..18): Electric cyan-blue wave
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(0   * r1_val);
        pixels_g[i] = (uint8_t)(200 * r1_val);
        pixels_b[i] = (uint8_t)(255 * r1_val);
    }

    // Ring 2 (19..42): Blazing solar gold outer horizon
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(255 * r2_val);
        pixels_g[i] = (uint8_t)(160 * r2_val);
        pixels_b[i] = (uint8_t)(20  * r2_val);
    }

#if CELESTIAL_FOUR_RINGS
    // Ring 3 (43..58): Radiant electric violet shockwave (black between pulses)
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        if (r3_val > 0.05f) {
            pixels_r[i] = (uint8_t)(180 * r3_val);
            pixels_g[i] = (uint8_t)(30  * r3_val);
            pixels_b[i] = (uint8_t)(255 * r3_val);
        } else {
            pixels_r[i] = 0; pixels_g[i] = 0; pixels_b[i] = 0;
        }
    }
#else
    (void)r3_val;
#endif
}

// 5. Living Solar Plasma & Fire: organic, undulating celestial heat flow across all 59 LEDs
static void render_plasma(uint32_t tick) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        float angle = (float)i * 6.2831853f / (float)LED_RING_COUNT;
        float w1 = sinf(angle * 3.0f + (float)tick * 0.07f);
        float w2 = sinf(angle * 2.0f - (float)tick * 0.04f);
        float heat_f = 0.5f + 0.3f * w1 + 0.2f * w2;
        uint8_t heat = (uint8_t)(130 + 120.0f * heat_f);
        pixels_r[i] = heat;
        pixels_g[i] = (uint8_t)((heat > 160) ? ((heat - 160) * 1.6f) : 0);
        pixels_b[i] = (uint8_t)((heat > 230) ? 25 : 0);
    }
}

// 6. Starry Night / Stardust: deep navy backdrop with twinkling diamond sparkles across all 59 LEDs
static void render_stars(uint32_t tick) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 8;
        pixels_g[i] = 14;
        pixels_b[i] = 60;
    }

    int count = LED_RING_COUNT;
    int star1 = (tick / 4) % count;
    int phase1 = tick % 4;
    float burst1 = 1.0f - ((float)phase1 / 4.0f);
    pixels_r[star1] = (uint8_t)(8  + 170 * burst1);
    pixels_g[star1] = (uint8_t)(14 + 165 * burst1);
    pixels_b[star1] = (uint8_t)(60 + 135 * burst1);

    int star2 = (tick / 7 + 19) % count;
    int phase2 = tick % 7;
    float burst2 = 1.0f - ((float)phase2 / 7.0f);
    pixels_r[star2] = (uint8_t)(8  + 150 * burst2);
    pixels_g[star2] = (uint8_t)(14 + 160 * burst2);
    pixels_b[star2] = (uint8_t)(60 + 135 * burst2);

    int star3 = (tick / 5 + 31) % count;
    int phase3 = tick % 5;
    float burst3 = 1.0f - ((float)phase3 / 5.0f);
    pixels_r[star3] = (uint8_t)(8  + 160 * burst3);
    pixels_g[star3] = (uint8_t)(14 + 150 * burst3);
    pixels_b[star3] = (uint8_t)(60 + 135 * burst3);
}

// 7. Live Audio / Voice Reactive: responds dynamically across concentric rings to mic level
static void render_voice_reactive(uint8_t mic_level_pct) {
    if (mic_level_pct > 100) mic_level_pct = 100;
    float level = (float)mic_level_pct / 100.0f; // 0.0 to 1.0

    // Center LED reflects instant vocal energy: Soft blue at idle -> blazing gold/white at loud
    pixels_r[0] = (uint8_t)(20  + level * 235);
    pixels_g[0] = (uint8_t)(40  + level * 215);
    pixels_b[0] = (uint8_t)(160 + (1.0f - level) * 95);

#if CELESTIAL_FOUR_RINGS
    // Ring 0 (1..6): 0..25% mic level
    float r0_progress = level / 0.25f;
    if (r0_progress > 1.0f) r0_progress = 1.0f;
    int r0_active = (int)(r0_progress * 6.0f);
    for (int i = 1; i <= 6; i++) {
        if (i <= r0_active) {
            pixels_r[i] = (uint8_t)(level * 220);
            pixels_g[i] = (uint8_t)(255 - level * 80);
            pixels_b[i] = (uint8_t)(255);
        } else {
            pixels_r[i] = 4; pixels_g[i] = 10; pixels_b[i] = 25;
        }
    }

    // Ring 1 (7..18): 25..50% mic level
    float r1_progress = (level > 0.25f) ? ((level - 0.25f) / 0.25f) : 0.0f;
    if (r1_progress > 1.0f) r1_progress = 1.0f;
    int r1_active = (int)(r1_progress * 12.0f);
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int idx = i - 7;
        if (idx < r1_active) {
            pixels_r[i] = (uint8_t)(0   + r1_progress * 255);
            pixels_g[i] = (uint8_t)(220 - r1_progress * 80);
            pixels_b[i] = (uint8_t)(255);
        } else {
            pixels_r[i] = 4; pixels_g[i] = 8; pixels_b[i] = 22;
        }
    }

    // Ring 3 (43..58, 16 LEDs): 50..75% mic level (pure black at idle/low level)
    unsigned lvl_u = (unsigned)mic_level_pct;
    unsigned r3_progress_pct = (lvl_u <= 50) ? 0 : (lvl_u >= 75) ? 100 : ((lvl_u - 50) * 4);
    int r3_filled = (int)(r3_progress_pct * 16 / 100);
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        int idx = i - 43;
        if (idx < r3_filled) {
            uint16_t hue = (uint16_t)(190 - r3_progress_pct * 150 / 100);
            uint8_t val = (uint8_t)(100 + r3_progress_pct * 80 / 100);
            hsv_to_rgb(hue, 255, val, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
        } else {
            pixels_r[i] = 0; pixels_g[i] = 0; pixels_b[i] = 0;
        }
    }

    // Ring 2 (19..42, 24 LEDs): 75..100% outer climax
    float r2_power = (level > 0.75f) ? ((level - 0.75f) / 0.25f) : 0.0f;
    if (r2_power > 1.0f) r2_power = 1.0f;
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        if (r2_power > 0.05f) {
            pixels_r[i] = (uint8_t)(255 * r2_power);
            pixels_g[i] = (uint8_t)(40  * r2_power);
            pixels_b[i] = (uint8_t)(220 * r2_power);
        } else {
            pixels_r[i] = 3; pixels_g[i] = 6; pixels_b[i] = 18;
        }
    }
#else
    // 3 Concentric Rings (43 LEDs: 7 Core + 12 Inner + 24 Outer):
    // Ring 0 (1..6): 0..30% mic level
    float r0_progress = level / 0.30f;
    if (r0_progress > 1.0f) r0_progress = 1.0f;
    int r0_active = (int)(r0_progress * 6.0f);
    for (int i = 1; i <= 6; i++) {
        if (i <= r0_active) {
            pixels_r[i] = (uint8_t)(level * 220);
            pixels_g[i] = (uint8_t)(255 - level * 80);
            pixels_b[i] = (uint8_t)(255);
        } else {
            pixels_r[i] = 4; pixels_g[i] = 10; pixels_b[i] = 25;
        }
    }

    // Ring 1 (7..18, 12 LEDs): 30..65% mic level
    float r1_progress = (level > 0.30f) ? ((level - 0.30f) / 0.35f) : 0.0f;
    if (r1_progress > 1.0f) r1_progress = 1.0f;
    int r1_active = (int)(r1_progress * 12.0f);
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int idx = i - 7;
        if (idx < r1_active) {
            pixels_r[i] = (uint8_t)(0   + r1_progress * 255);
            pixels_g[i] = (uint8_t)(220 - r1_progress * 80);
            pixels_b[i] = (uint8_t)(255);
        } else {
            pixels_r[i] = 4; pixels_g[i] = 8; pixels_b[i] = 22;
        }
    }

    // Ring 2 (19..42, 24 LEDs): 65..100% outer horizon
    float r2_power = (level > 0.65f) ? ((level - 0.65f) / 0.35f) : 0.0f;
    if (r2_power > 1.0f) r2_power = 1.0f;
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        if (r2_power > 0.05f) {
            pixels_r[i] = (uint8_t)(255 * r2_power);
            pixels_g[i] = (uint8_t)(40  * r2_power);
            pixels_b[i] = (uint8_t)(220 * r2_power);
        } else {
            pixels_r[i] = 3; pixels_g[i] = 6; pixels_b[i] = 18;
        }
    }
#endif
}

// 8. Synchronized Music & Cosmic SFX Visualizer across all 4 concentric rings:
static void render_chiptune_sync(uint32_t tick) {
    float f = audio_get_current_note_freq();
    if (f > 80.0f) {
        int hue = (int)((f - 100.0f) * 0.35f);
        if (hue < 0) hue = 0;
        if (hue > 359) hue = (hue % 360);

        uint8_t r, g, b;
        hsv_to_rgb((uint16_t)hue, 255, 180, &r, &g, &b);

        // Center LED (0): Strobe flash on every note hit
        pixels_r[0] = (uint8_t)((r * 2 + 255) / 3);
        pixels_g[0] = (uint8_t)((g * 2 + 255) / 3);
        pixels_b[0] = (uint8_t)((b * 2 + 255) / 3);

        // Ring 0 (1..6): Frequency-reactive spinning wheel
        int active_r0 = 1 + (tick % 6);
        for (int i = 1; i <= 6; i++) {
            if (i == active_r0 || i == (active_r0 % 6) + 1) {
                pixels_r[i] = r; pixels_g[i] = g; pixels_b[i] = b;
            } else {
                pixels_r[i] = r / 5; pixels_g[i] = g / 5; pixels_b[i] = b / 5;
            }
        }

        // Ring 1 (7..18): Dual rotating beacons spinning to the beat
        int b1 = 7 + ((tick * 2) % 12);
        int b2 = 7 + ((tick * 2 + 6) % 12);
        for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
            if (i == b1 || i == b2) {
                pixels_r[i] = 0; pixels_g[i] = 230; pixels_b[i] = 255; // Electric cyan beacon
            } else {
                pixels_r[i] = r / 6; pixels_g[i] = g / 6; pixels_b[i] = b / 6;
            }
        }

        // Ring 2 (19..42): Outer horizon radial ripple / VU meter
        int outer_step = (tick * 3) % 24;
        for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
            int dist = (19 + outer_step - i + 24) % 24;
            if (dist < 4) {
                pixels_r[i] = (uint8_t)(r * (4 - dist) / 4);
                pixels_g[i] = (uint8_t)(g * (4 - dist) / 4);
                pixels_b[i] = (uint8_t)(b * (4 - dist) / 4);
            } else {
                pixels_r[i] = 6; pixels_g[i] = 8; pixels_b[i] = 22;
            }
        }

#if CELESTIAL_FOUR_RINGS
        // Ring 3 (43..58): Harmonic counter-rotating orbital beacon
        int b3 = 43 + ((tick * 2) % 16);
        int b4 = 43 + ((tick * 2 + 8) % 16);
        for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
            if (i == b3 || i == b4) {
                pixels_r[i] = 220; pixels_g[i] = 40; pixels_b[i] = 255; // Electric magenta beacon
            } else {
                pixels_r[i] = r / 6; pixels_g[i] = g / 6; pixels_b[i] = b / 6;
            }
        }
#endif
    } else {
        // Rest / pause between notes: subtle cosmic stardust glow
        for (int i = 0; i < LED_RING_COUNT; i++) {
            pixels_r[i] = 8;
            pixels_g[i] = 12;
            pixels_b[i] = 28;
        }
    }
}

// 9. Solar System Orrery planetary orbits on 4 physical concentric rings:
//    - Ring 0 (0..6): Sun (blazing warm solar gold & corona flare)
//    - Ring 1 (7..18, 12 LEDs): Earth's orbit! Earth is a bright sapphire blue dot, Moon is dim white, with "HERE" locator pulse
//    - Ring 2 (19..42, 24 LEDs): Mars orbit (rust red) & asteroid belt starlight
//    - Ring 3 (43..58, 16 LEDs): Jupiter (warm amber/gold giant) & Galilean moons Io and Europa
static void render_solar(uint32_t tick) {
    // 1. Center Ring 0 (0..6): The Sun (Sol)
    float sun_flare = (sinf((float)tick * 0.12f) + 1.0f) * 0.5f;
    pixels_r[0] = 255;
    pixels_g[0] = (uint8_t)(190 + 50.0f * sun_flare);
    pixels_b[0] = (uint8_t)(30  + 30.0f * sun_flare);

    for (int i = 1; i <= 6; i++) {
        float f = (sinf((float)(tick * 4 + i * 60) * 0.05f) + 1.0f) * 0.5f;
        pixels_r[i] = (uint8_t)(220 + 35.0f * f);
        pixels_g[i] = (uint8_t)(100 + 40.0f * f);
        pixels_b[i] = (uint8_t)(10  + 20.0f * f);
    }

    // 2. Ring 1 (7..18, 12 LEDs): Earth's Orbit!
    int earth_idx = 7 + ((tick / 3) % 12);
    int moon_idx  = 7 + ((earth_idx - 7 + 1) % 12);
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        if (i == earth_idx) {
            // Earth (TERRA) - Sapphire Blue with locator beacon flash
            bool pulse = ((tick / 2) % 4) == 0;
            if (pulse) {
                pixels_r[i] = 120; pixels_g[i] = 255; pixels_b[i] = 240;
            } else {
                pixels_r[i] = 0;   pixels_g[i] = 160; pixels_b[i] = 255;
            }
        } else if (i == moon_idx) {
            // Moon - gentle dim lunar white
            pixels_r[i] = 140; pixels_g[i] = 160; pixels_b[i] = 190;
        } else {
            // Deep space background
            pixels_r[i] = 1; pixels_g[i] = 2; pixels_b[i] = 8;
        }
    }

    // 3. Ring 2 (19..42, 24 LEDs): Mars & Asteroid Belt Orbit!
    int mars_idx = 19 + ((tick / 6) % 24);
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        if (i == mars_idx) {
            // Mars - Rust Red / Scarlet
            pixels_r[i] = 255; pixels_g[i] = 45; pixels_b[i] = 15;
        } else {
            if ((i * 7 + tick) % 11 == 0) {
                pixels_r[i] = 20; pixels_g[i] = 25; pixels_b[i] = 35; // Dim asteroid
            } else {
                pixels_r[i] = 1;  pixels_g[i] = 1;  pixels_b[i] = 6;  // Deep space
            }
        }
    }

#if CELESTIAL_FOUR_RINGS
    // 4. Ring 3 (43..58, 16 LEDs): Jupiter & Galilean Moons!
    int jup_pos = 43 + ((tick / 8) % 16);
    int io_pos  = 43 + ((jup_pos - 43 + 1) % 16);
    int eur_pos = 43 + ((jup_pos - 43 + 15) % 16);
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        if (i == jup_pos) {
            // Jupiter: Majestic solar amber/gold gas giant
            pixels_r[i] = 255; pixels_g[i] = 160; pixels_b[i] = 20;
        } else if (i == io_pos) {
            // Io: Volcanic sulfur yellow moon
            pixels_r[i] = 200; pixels_g[i] = 180; pixels_b[i] = 30;
        } else if (i == eur_pos) {
            // Europa: Glacial ice cyan moon
            pixels_r[i] = 120; pixels_g[i] = 200; pixels_b[i] = 255;
        } else {
            // Pure velvet outer space
            pixels_r[i] = 0; pixels_g[i] = 0; pixels_b[i] = 0;
        }
    }
#endif
}

#if CELESTIAL_FOUR_RINGS && CELESTIAL_RING_EXPERIMENT
// Diagnostic build only: pure RGB cycle on spare during RAINBOW
static void render_spare_ring(uint32_t tick, int mode, uint8_t mic_level_pct, bool music_active) {
    (void)tick; (void)mic_level_pct;
    if (!music_active && (mode == LED_ANIM_RAINBOW || mode < 0 || mode >= LED_ANIM_COUNT)) {
        int64_t elapsed = absolute_time_diff_us(led_boot_test_started, get_absolute_time()) -
                          (int64_t)LED_BOOT_COLOR_MS * 3000;
        int phase = elapsed > 0 ? (int)((elapsed / ((int64_t)LED_DIAG_COLOR_MS * 1000)) % 3) : 0;
        for (int i = LED_BASE_CHAIN_COUNT; i < LED_RING_COUNT; ++i) {
            pixels_r[i] = phase == 0 ? 180 : 0;
            pixels_g[i] = phase == 1 ? 180 : 0;
            pixels_b[i] = phase == 2 ? 180 : 0;
        }
    }
}
#endif

void leds_update(bool easter_egg_active, bool led_menu_active, int selected_anim_idx, uint8_t mic_level_pct) {
    (void)led_menu_active;
    // 33 Hz animation refresh rate (30 ms)
    if (absolute_time_diff_us(next_anim_time, get_absolute_time()) < 30000) {
        return;
    }
    next_anim_time = get_absolute_time();
    anim_tick++;

#if CELESTIAL_FOUR_RINGS
    int64_t boot_elapsed_us = absolute_time_diff_us(led_boot_test_started, get_absolute_time());
    if (boot_elapsed_us >= 0 && boot_elapsed_us < (int64_t)LED_BOOT_COLOR_MS * 3000) {
        int phase = (int)(boot_elapsed_us / ((int64_t)LED_BOOT_COLOR_MS * 1000));
        if (phase != led_boot_test_phase) {
            led_boot_test_phase = phase;
            LOG_INFO("LED", "DMA boot color:%s | frames:%u | brightness:%u%%",
                     phase == 0 ? "RED" : phase == 1 ? "GREEN" : "BLUE",
                     (unsigned)LED_RING_COUNT, (unsigned)master_brightness);
        }
        leds_set_all_rgb(phase == 0 ? 255 : 0, phase == 1 ? 255 : 0, phase == 2 ? 255 : 0);
        return;
    }
    if (led_boot_test_phase != 3) {
        led_boot_test_phase = 3;
        LOG_INFO("LED", "DMA boot RGB complete; four-ring effects active");
    }
#endif

    bool music_active = audio_is_chiptune_playing() || audio_is_cosmic_sfx_playing();
    int current_mode = selected_anim_idx;
    if (easter_egg_active) {
        // Hyperspeed 67 easter egg rainbow vortex!
        uint16_t fast_hue = (anim_tick * 16) % 360;
        for (int i = 0; i < LED_RING_COUNT; i++) {
            uint16_t led_hue = (fast_hue + (i * 360 / LED_RING_COUNT)) % 360;
            hsv_to_rgb(led_hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
        }
    } else if (music_active) {
        // Active chiptune and cosmic SFX synchronization across all 4 rings!
        render_chiptune_sync(anim_tick);
    } else {
        switch (current_mode) {
            case LED_ANIM_BREATHE:
                render_breathe(anim_tick);
                break;
            case LED_ANIM_VORTEX:
                render_vortex(anim_tick, mic_level_pct);
                break;
            case LED_ANIM_SNAKE:
                render_snake(anim_tick);
                break;
            case LED_ANIM_RAINBOW:
                render_rainbow_wave(anim_tick);
                break;
            case LED_ANIM_PULSAR:
                render_pulsar(anim_tick);
                break;
            case LED_ANIM_PLASMA:
                render_plasma(anim_tick);
                break;
            case LED_ANIM_STARS:
                render_stars(anim_tick);
                break;
            case LED_ANIM_VOICE:
                render_voice_reactive(mic_level_pct);
                break;
            case LED_ANIM_SOLAR:
                render_solar(anim_tick);
                break;
            case LED_ANIM_DATA_RECV:
                render_data_recv(anim_tick);
                break;
            case LED_ANIM_EXPAND_SPEECH:
                render_expand_speech(anim_tick);
                break;
            default:
                render_rainbow_wave(anim_tick);
                break;
        }
    }

#if CELESTIAL_FOUR_RINGS && CELESTIAL_RING_EXPERIMENT
    if (!easter_egg_active) render_spare_ring(anim_tick, current_mode, mic_level_pct, music_active);
#endif
    show_pixels();
}
