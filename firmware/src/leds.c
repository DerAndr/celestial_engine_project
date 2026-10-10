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

static uint8_t master_brightness = 18; // Default 18% (safe low-current, gentle on eyes)
static absolute_time_t next_anim_time;
static uint32_t anim_tick = 0;
static int active_anim_mode = LED_ANIM_VORTEX;
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
    "PULSAR PULSE",
    "SOLAR PLASMA",
    "STARRY NIGHT",
    "VOICE REACTIVE",
    "RAINBOW WAVE"
};

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t r_scaled = (uint32_t)(r * master_brightness) / 100;
    uint32_t g_scaled = (uint32_t)(g * master_brightness) / 100;
    uint32_t b_scaled = (uint32_t)(b * master_brightness) / 100;
    return ((uint32_t)g_scaled << 16) | ((uint32_t)r_scaled << 8) | (uint32_t)b_scaled;
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put(led_pio, led_sm, pixel_grb << 8u);
}

static void show_pixels(void) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        put_pixel(rgb_to_grb(pixels_r[i], pixels_g[i], pixels_b[i]));
    }
    sleep_us(80); // Reset latch (>50 us)
}

// Simple HSV to RGB converter (h: 0..359, s: 0..255, v: 0..255)
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }
    uint8_t region = (h % 360) / 60;
    uint8_t remainder = ((h % 360) - (region * 60)) * 6;
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
    if (brightness_pct > 100) brightness_pct = 100;
    master_brightness = brightness_pct;
}

// -------------------------------------------------------------
// Core Animation Renderers
// -------------------------------------------------------------

// 0. Slow cosmic breathing in active color
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

// 1. Spinning Galaxy Comet Vortex: center pulses, comet orbits 6 outer LEDs in a crisp, visible circle
static void render_vortex(uint32_t tick) {
    // Advances every 3 ticks (~90 ms per step at 33 Hz): clear, distinctly visible circular motion!
    int head = 1 + ((tick / 3) % 6); // Outer ring LEDs are 1..6

    // Center LED (index 0) pulses gently in warm celestial amber
    float c_pulse = (sinf((float)tick * 0.08f) + 1.0f) * 0.5f;
    pixels_r[0] = (uint8_t)(255 * (0.35f + 0.65f * c_pulse));
    pixels_g[0] = (uint8_t)(150 * (0.35f + 0.65f * c_pulse));
    pixels_b[0] = (uint8_t)(35  * (0.35f + 0.65f * c_pulse));

    // Outer 6 LEDs: bright head with a dynamic, glowing comet trail orbiting the circle
    for (int i = 1; i <= 6; i++) {
        int dist = (head - i + 6) % 6; // 0 = head, 1 = tail 1, etc.

        if (dist == 0) {
            // Comet Head: brilliant diamond cyan/white
            pixels_r[i] = 180;
            pixels_g[i] = 255;
            pixels_b[i] = 255;
        } else if (dist == 1) {
            // Tail 1: vivid electric cyan
            pixels_r[i] = 0;
            pixels_g[i] = 210;
            pixels_b[i] = 255;
        } else if (dist == 2) {
            // Tail 2: deep cosmic blue
            pixels_r[i] = 20;
            pixels_g[i] = 60;
            pixels_b[i] = 240;
        } else if (dist == 3) {
            // Tail 3: violet ember
            pixels_r[i] = 100;
            pixels_g[i] = 0;
            pixels_b[i] = 180;
        } else {
            // Resting background stardust
            pixels_r[i] = 8;
            pixels_g[i] = 10;
            pixels_b[i] = 25;
        }
    }
}

// 2. Cosmic Heartbeat / Pulsar: 'ba-dum' double pulse from center outward
static void render_pulsar(uint32_t tick) {
    uint32_t phase = tick % 90; // Repeat every 3 seconds (30 Hz * 3)

    float c_val = 0.05f;
    float ring_val = 0.05f;

    // Pulse 1: 'ba' (ticks 0..12)
    if (phase < 6) {
        c_val = 1.0f - ((float)phase / 6.0f);
    } else if (phase >= 4 && phase < 12) {
        ring_val = 1.0f - ((float)(phase - 4) / 8.0f);
    }
    // Pulse 2: 'dum' (ticks 18..34)
    else if (phase >= 18 && phase < 24) {
        c_val = 1.0f - ((float)(phase - 18) / 6.0f);
    } else if (phase >= 22 && phase < 34) {
        ring_val = 1.0f - ((float)(phase - 22) / 12.0f);
    }

    // Center LED: Brilliant white-cyan core
    pixels_r[0] = (uint8_t)(180 * c_val);
    pixels_g[0] = (uint8_t)(240 * c_val);
    pixels_b[0] = (uint8_t)(255 * c_val);

    // Outer ring: Deep violet-magenta shockwave
    for (int i = 1; i <= 6; i++) {
        pixels_r[i] = (uint8_t)(240 * ring_val);
        pixels_g[i] = (uint8_t)(40 * ring_val);
        pixels_b[i] = (uint8_t)(255 * ring_val);
    }
}

// 3. Living Solar Plasma & Fire: organic flickering amber/crimson/gold flames
static void render_plasma(uint32_t tick) {
    (void)tick;
    for (int i = 0; i < LED_RING_COUNT; i++) {
        // Random organic heat flicker
        int heat = 120 + (rand() % 136); // 120..255
        // Fire palette: high heat = bright gold, lower heat = deep crimson
        pixels_r[i] = (uint8_t)heat;
        pixels_g[i] = (uint8_t)((heat > 160) ? ((heat - 160) * 1.8f) : 0);
        pixels_b[i] = (uint8_t)((heat > 230) ? 40 : 0);
    }
}

// 4. Starry Night / Stardust: deep navy backdrop with twinkling diamond sparkles
static void render_stars(uint32_t tick) {
    // Ambient starlit sky background
    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 10;
        pixels_g[i] = 20;
        pixels_b[i] = 80;
    }

    // Every few ticks, a star twinkles brightly
    int twinkle_led = (tick / 5) % LED_RING_COUNT;
    int phase = tick % 5;
    float burst = 1.0f - ((float)phase / 5.0f);

    pixels_r[twinkle_led] = (uint8_t)(10 + 245 * burst);
    pixels_g[twinkle_led] = (uint8_t)(20 + 235 * burst);
    pixels_b[twinkle_led] = (uint8_t)(80 + 175 * burst);
}

// 5. Live Audio / Voice Reactive: responds dynamically to INMP441 microphone level!
static void render_voice_reactive(uint8_t mic_level_pct) {
    if (mic_level_pct > 100) mic_level_pct = 100;
    float level = (float)mic_level_pct / 100.0f; // 0.0 to 1.0

    // Center LED reflects instant energy: Soft blue at idle, fiery gold/white at loud
    pixels_r[0] = (uint8_t)(20 + level * 235);
    pixels_g[0] = (uint8_t)(40 + level * 215);
    pixels_b[0] = (uint8_t)(160 + (1.0f - level) * 95);

    // Outer ring expands outward with volume
    int active_ring_leds = (int)(level * 6.9f);
    for (int i = 1; i <= 6; i++) {
        if (i <= active_ring_leds) {
            // Active voice flare: intense emerald-cyan to magenta
            pixels_r[i] = (uint8_t)(level * 255);
            pixels_g[i] = (uint8_t)((1.0f - level * 0.5f) * 255);
            pixels_b[i] = (uint8_t)(255);
        } else {
            // Idle ambient glow
            pixels_r[i] = 5;
            pixels_g[i] = 15;
            pixels_b[i] = 40;
        }
    }
}

// 6. Smooth Chromatic Rainbow Wave
static void render_rainbow_wave(uint32_t tick) {
    uint16_t base_hue = (tick * 4) % 360;
    for (int i = 0; i < LED_RING_COUNT; i++) {
        uint16_t led_hue = (base_hue + (i * 360 / LED_RING_COUNT)) % 360;
        hsv_to_rgb(led_hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
    }
}

// 7. Synchronized 8-Bit Chiptune Disco: pulses and rotates in exact rhythm with melody!
static void render_chiptune_sync(uint32_t tick) {
    float f = audio_get_current_note_freq();
    if (f > 80.0f) {
        // Map frequency 160..900 Hz to hue 0..300 (bass=red/amber, mid=green/cyan, high=magenta/cyan)
        int hue = (int)((f - 160.0f) * 0.40f);
        if (hue < 0) hue = 0;
        if (hue > 359) hue = (hue % 360);

        uint8_t r, g, b;
        hsv_to_rgb((uint16_t)hue, 255, 255, &r, &g, &b);

        // Center LED bursts with bright diamond-tinted note flash
        pixels_r[0] = (uint8_t)((r * 2 + 255) / 3);
        pixels_g[0] = (uint8_t)((g * 2 + 255) / 3);
        pixels_b[0] = (uint8_t)((b * 2 + 255) / 3);

        // Outer ring spins fast to the rhythm!
        int active_led = 1 + ((tick) % 6);
        for (int i = 1; i <= 6; i++) {
            if (i == active_led || i == (active_led % 6) + 1) {
                pixels_r[i] = r;
                pixels_g[i] = g;
                pixels_b[i] = b;
            } else {
                pixels_r[i] = r / 6;
                pixels_g[i] = g / 6;
                pixels_b[i] = b / 6;
            }
        }
    } else {
        // Note rest / gap: subtle resting stardust glow
        for (int i = 0; i < LED_RING_COUNT; i++) {
            pixels_r[i] = 12;
            pixels_g[i] = 15;
            pixels_b[i] = 35;
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
    } else if (audio_is_chiptune_playing()) {
        // Active 8-bit chiptune synchronization!
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
            case LED_ANIM_RAINBOW:
            default:
                render_rainbow_wave(anim_tick);
                break;
        }
    }

    show_pixels();
}
