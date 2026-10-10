#include "leds.h"
#include "audio.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
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
    "VOICE REACTIVE"
};

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t r_scaled = (uint32_t)(r * master_brightness) / 100;
    uint32_t g_scaled = (uint32_t)(g * master_brightness) / 100;
    uint32_t b_scaled = (uint32_t)(b * master_brightness) / 100;
    return ((uint32_t)g_scaled << 16) | ((uint32_t)r_scaled << 8) | (uint32_t)b_scaled;
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(led_pio, led_sm, pixel_grb << 8u);
}

static void show_pixels(void) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        put_pixel(rgb_to_grb(pixels_r[i], pixels_g[i], pixels_b[i]));
    }
    // FIFO empty can still leave one 24-bit pixel in the output shifter.
    while (!pio_sm_is_tx_fifo_empty(led_pio, led_sm)) {
        tight_loop_contents();
    }
    sleep_us(30 + 80); // Last pixel at 800 kHz, then reset latch (>50 us).
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

    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 0;
        pixels_g[i] = 0;
        pixels_b[i] = 0;
    }
    show_pixels();
    next_anim_time = get_absolute_time();
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

// 1. Spinning Galaxy Comet Vortex:
//    - Center (0): Pulsing solar ember
//    - Ring 0 (1..6): Clockwise comet
//    - Ring 1 (7..18): Counter-rotating electric cyan comet
//    - Ring 2 (19..42): Clockwise outer solar horizon comet
static void render_vortex(uint32_t tick) {
    // Center LED (index 0) pulses gently in warm celestial amber
    float c_pulse = (sinf((float)tick * 0.08f) + 1.0f) * 0.5f;
    pixels_r[0] = (uint8_t)(255 * (0.35f + 0.65f * c_pulse));
    pixels_g[0] = (uint8_t)(150 * (0.35f + 0.65f * c_pulse));
    pixels_b[0] = (uint8_t)(35  * (0.35f + 0.65f * c_pulse));

    // Ring 0 (1..6): 6 LEDs clockwise comet
    int head0 = 1 + ((tick / 2) % 6);
    for (int i = 1; i <= 6; i++) {
        int dist = (head0 - i + 6) % 6;
        if (dist == 0) {
            pixels_r[i] = 255; pixels_g[i] = 255; pixels_b[i] = 255; // Diamond white head
        } else if (dist == 1) {
            pixels_r[i] = 0;   pixels_g[i] = 220; pixels_b[i] = 255; // Cyan trail
        } else if (dist == 2) {
            pixels_r[i] = 30;  pixels_g[i] = 80;  pixels_b[i] = 240; // Deep blue
        } else {
            pixels_r[i] = 8;   pixels_g[i] = 10;  pixels_b[i] = 25;  // Cosmic dust
        }
    }

    // Ring 1 (7..18): 12 LEDs counter-rotating cyan comet
    int step1 = (tick / 2) % 12;
    int head1 = 7 + ((12 - step1) % 12);
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int dist = (head1 - i + 12) % 12;
        if (dist == 0) {
            pixels_r[i] = 0;   pixels_g[i] = 255; pixels_b[i] = 200; // Electric teal head
        } else if (dist == 1 || dist == 2) {
            pixels_r[i] = 0;   pixels_g[i] = 130; pixels_b[i] = 220; // Ocean trail
        } else if (dist == 3) {
            pixels_r[i] = 80;  pixels_g[i] = 0;   pixels_b[i] = 200; // Violet ember
        } else {
            pixels_r[i] = 6;   pixels_g[i] = 12;  pixels_b[i] = 30;  // Deep stardust
        }
    }

    // Ring 2 (19..42): 24 LEDs clockwise outer orbit
    int head2 = 19 + ((tick) % 24);
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        int dist = (head2 - i + 24) % 24;
        if (dist == 0) {
            pixels_r[i] = 255; pixels_g[i] = 180; pixels_b[i] = 0;   // Solar gold head
        } else if (dist == 1 || dist == 2) {
            pixels_r[i] = 220; pixels_g[i] = 60;  pixels_b[i] = 20;  // Amber flame trail
        } else if (dist == 3 || dist == 4) {
            pixels_r[i] = 120; pixels_g[i] = 0;   pixels_b[i] = 160; // Magenta trail
        } else {
            pixels_r[i] = 8;   pixels_g[i] = 8;   pixels_b[i] = 24;  // Horizon night
        }
    }
}

// 2. Cosmic Snake: Mythical dragon serpent slithering across all 43 LEDs
//    Head is brilliant incandescent diamond-gold; long trailing rainbow tail (12 LEDs)
static void render_snake(uint32_t tick) {
    int head = tick % LED_RING_COUNT;

    for (int i = 0; i < LED_RING_COUNT; i++) {
        int dist = (head - i + LED_RING_COUNT) % LED_RING_COUNT;

        if (dist == 0) {
            // Snake Head: Brilliant gleaming diamond white-gold
            pixels_r[i] = 255;
            pixels_g[i] = 250;
            pixels_b[i] = 220;
        } else if (dist >= 1 && dist <= 12) {
            // Rainbow Serpent Body with smooth exponential fade
            float fade = 1.0f - ((float)dist / 13.0f);
            uint16_t hue = (tick * 6 + dist * 26) % 360;
            hsv_to_rgb(hue, 255, (uint8_t)(255 * fade), &pixels_r[i], &pixels_g[i], &pixels_b[i]);
        } else {
            // Resting cosmic starlight so all 3 rings remain subtly visible in the dark
            pixels_r[i] = 3;
            pixels_g[i] = 5;
            pixels_b[i] = 16;
        }
    }
}

// 3. Smooth Concentric Chromatic Rainbow Wave:
//    Radiating multi-ring spiral with coordinated angular phases across 7, 12, and 24 rings
static void render_rainbow_wave(uint32_t tick) {
    uint16_t base_hue = (tick * 4) % 360;

    // Center Core LED (0)
    hsv_to_rgb(base_hue, 255, 255, &pixels_r[0], &pixels_g[0], &pixels_b[0]);

    // Ring 0 (1..6): 6 LEDs, 60-degree increments
    for (int i = 1; i <= 6 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + (i - 1) * 60) % 360;
        hsv_to_rgb(hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }

    // Ring 1 (7..18): 12 LEDs, 30-degree increments, shifted by 50 deg
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + 50 + (i - 7) * 30) % 360;
        hsv_to_rgb(hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }

    // Ring 2 (19..42): 24 LEDs, 15-degree increments, shifted by 100 deg
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        uint16_t hue = (base_hue + 100 + (i - 19) * 15) % 360;
        hsv_to_rgb(hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }
}

// 4. Cosmic Heartbeat / Pulsar: 'ba-dum' shockwave expanding outward through all 3 rings
static void render_pulsar(uint32_t tick) {
    uint32_t phase = tick % 90; // Repeat every 3 seconds (30 Hz * 3)

    float c_val = 0.05f;
    float r0_val = 0.05f;
    float r1_val = 0.05f;
    float r2_val = 0.05f;

    // Pulse 1: 'ba' (expanding from center outward)
    if (phase < 6) {
        c_val = 1.0f - ((float)phase / 6.0f);
    } else if (phase >= 4 && phase < 11) {
        r0_val = 1.0f - ((float)(phase - 4) / 7.0f);
    } else if (phase >= 8 && phase < 16) {
        r1_val = 1.0f - ((float)(phase - 8) / 8.0f);
    } else if (phase >= 12 && phase < 22) {
        r2_val = 1.0f - ((float)(phase - 12) / 10.0f);
    }

    // Pulse 2: 'dum' (larger shockwave)
    if (phase >= 20 && phase < 27) {
        c_val = 1.0f - ((float)(phase - 20) / 7.0f);
    } else if (phase >= 24 && phase < 32) {
        r0_val = 1.0f - ((float)(phase - 24) / 8.0f);
    } else if (phase >= 28 && phase < 38) {
        r1_val = 1.0f - ((float)(phase - 28) / 10.0f);
    } else if (phase >= 34 && phase < 46) {
        r2_val = 1.0f - ((float)(phase - 34) / 12.0f);
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
}

// 5. Living Solar Plasma & Fire: organic flickering amber/crimson/gold flames
static void render_plasma(uint32_t tick) {
    (void)tick;
    for (int i = 0; i < LED_RING_COUNT; i++) {
        // Random organic heat flicker
        int heat = 120 + (rand() % 136); // 120..255
        pixels_r[i] = (uint8_t)heat;
        pixels_g[i] = (uint8_t)((heat > 160) ? ((heat - 160) * 1.8f) : 0);
        pixels_b[i] = (uint8_t)((heat > 230) ? 40 : 0);
    }
}

// 6. Starry Night / Stardust: deep navy backdrop with twinkling diamond sparkles across all 43 LEDs
static void render_stars(uint32_t tick) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 8;
        pixels_g[i] = 14;
        pixels_b[i] = 60;
    }

    // Two stars twinkle at different intervals
    int star1 = (tick / 4) % LED_RING_COUNT;
    int phase1 = tick % 4;
    float burst1 = 1.0f - ((float)phase1 / 4.0f);
    pixels_r[star1] = (uint8_t)(8  + 247 * burst1);
    pixels_g[star1] = (uint8_t)(14 + 241 * burst1);
    pixels_b[star1] = (uint8_t)(60 + 195 * burst1);

    int star2 = (tick / 7 + 19) % LED_RING_COUNT;
    int phase2 = tick % 7;
    float burst2 = 1.0f - ((float)phase2 / 7.0f);
    pixels_r[star2] = (uint8_t)(8  + 200 * burst2);
    pixels_g[star2] = (uint8_t)(14 + 230 * burst2);
    pixels_b[star2] = (uint8_t)(60 + 195 * burst2);
}

// 7. Live Audio / Voice Reactive: responds dynamically across all 3 rings to microphone level
static void render_voice_reactive(uint8_t mic_level_pct) {
    if (mic_level_pct > 100) mic_level_pct = 100;
    float level = (float)mic_level_pct / 100.0f; // 0.0 to 1.0

    // Center LED reflects instant vocal energy: Soft blue at idle -> blazing gold/white at loud
    pixels_r[0] = (uint8_t)(20  + level * 235);
    pixels_g[0] = (uint8_t)(40  + level * 215);
    pixels_b[0] = (uint8_t)(160 + (1.0f - level) * 95);

    // Ring 0 (1..6): Lights up smoothly with low/medium voice (0..40%)
    int r0_active = (int)(level * 2.5f * 6.0f);
    for (int i = 1; i <= 6; i++) {
        if (i <= r0_active) {
            pixels_r[i] = (uint8_t)(level * 220);
            pixels_g[i] = (uint8_t)(255 - level * 80);
            pixels_b[i] = (uint8_t)(255);
        } else {
            pixels_r[i] = 4; pixels_g[i] = 10; pixels_b[i] = 25;
        }
    }

    // Ring 1 (7..18): Circular VU halo fills with medium/high voice (30..80%)
    float r1_progress = (level > 0.3f) ? ((level - 0.3f) / 0.5f) : 0.0f;
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

    // Ring 2 (19..42): Outer horizon bursts on loud peaks (>70%)
    float r2_power = (level > 0.7f) ? ((level - 0.7f) / 0.3f) : 0.0f;
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        if (r2_power > 0.05f) {
            pixels_r[i] = (uint8_t)(255 * r2_power);
            pixels_g[i] = (uint8_t)(40  * r2_power);
            pixels_b[i] = (uint8_t)(220 * r2_power);
        } else {
            pixels_r[i] = 3; pixels_g[i] = 6; pixels_b[i] = 18;
        }
    }
}

// 8. Synchronized Music & Cosmic SFX Visualizer:
//    Pulses, rotates, and radiates across all 3 concentric rings in exact rhythm with melody
static void render_chiptune_sync(uint32_t tick) {
    float f = audio_get_current_note_freq();
    if (f > 80.0f) {
        // Map frequency 100..1200 Hz to hue 0..300 (bass=red/gold, mids=green/cyan, highs=magenta/violet)
        int hue = (int)((f - 100.0f) * 0.35f);
        if (hue < 0) hue = 0;
        if (hue > 359) hue = (hue % 360);

        uint8_t r, g, b;
        hsv_to_rgb((uint16_t)hue, 255, 255, &r, &g, &b);

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
    } else {
        // Rest / pause between notes: subtle cosmic stardust glow
        for (int i = 0; i < LED_RING_COUNT; i++) {
            pixels_r[i] = 8;
            pixels_g[i] = 12;
            pixels_b[i] = 28;
        }
    }
}

void leds_update(bool easter_egg_active, bool led_menu_active, int selected_anim_idx, uint8_t mic_level_pct) {
    // 33 Hz animation refresh rate (30 ms)
    if (absolute_time_diff_us(next_anim_time, get_absolute_time()) < 30000) {
        return;
    }
    next_anim_time = get_absolute_time();
    anim_tick++;

    if (easter_egg_active) {
        // Hyperspeed 67 easter egg rainbow vortex!
        uint16_t fast_hue = (anim_tick * 16) % 360;
        for (int i = 0; i < LED_RING_COUNT; i++) {
            uint16_t led_hue = (fast_hue + (i * 360 / LED_RING_COUNT)) % 360;
            hsv_to_rgb(led_hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
        }
    } else if (audio_is_chiptune_playing() || audio_is_cosmic_sfx_playing()) {
        // Active chiptune and cosmic SFX synchronization across all 3 rings!
        render_chiptune_sync(anim_tick);
    } else {
        int current_mode = led_menu_active ? selected_anim_idx : active_anim_mode;

        switch (current_mode) {
            case LED_ANIM_BREATHE:
                render_breathe(anim_tick);
                break;
            case LED_ANIM_VORTEX:
                render_vortex(anim_tick);
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
            default:
                render_rainbow_wave(anim_tick);
                break;
        }
    }

    show_pixels();
}
