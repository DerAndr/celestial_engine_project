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
static int anim_ai_model = 0;
static int base_color_idx = 0;

void leds_set_ai_model(int model_idx) {
    anim_ai_model = model_idx;
}

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
    "SONIC EXPANSION",
    "MILKY WAY GALAXY",
    "TERRA PLANET",
    "HALLOWEEN SPOOK",
    "CELESTIAL CLOCK",
    "AETHER HOURGLASS",
    "CHROMA PRISMATIC",
    "CELESTIAL ORRERY",
    "OUROBOROS SNAKE",
    "RADIAL WAVE",
    "COLLIDING WAVES"
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

// ==============================================================================
// AI Model Personality LED Themes for Sending, Processing, and Speaking
// ==============================================================================
typedef struct {
    uint8_t r1, g1, b1; // Primary arm / wave color
    uint8_t r2, g2, b2; // Accent arm / telemetry color
    uint8_t cr, cg, cb; // Core flare / center burst color
} llm_led_theme_t;

static const llm_led_theme_t LLM_THEMES[5] = {
    // 0: DEEP THOUGHT (DeepSeek V3) - Cosmic Deep Ocean Cyan & Diamond Blue
    {   0, 170, 255,   15,  70, 240,  180, 245, 255 },
    // 1: EDDIE (Ministral 8B) - Sunny Solar Amber & Radiant Gold
    { 255, 170,  15,  255, 100,   0,  255, 235,  90 },
    // 2: GROK (Grok 4.5) - Cyberpunk Neon Magenta & Acid Violet / Mint Green
    { 255,  15, 160,  150,   0, 255,    0, 255, 160 },
    // 3: MARVIN (Llama 70B) - Melancholic Glacial Steel-Blue & Frosted Silver
    {  90, 140, 215,   50,  75, 160,  190, 210, 245 },
    // 4: LOCAL (Pi 5 Edge) - Cyber Emerald Matrix & Raspberry Ruby Core
    {  10, 255,  90,    0, 200,  70,  255,  15,  80 }
};

static inline const llm_led_theme_t* get_active_llm_theme(void) {
    int idx = (anim_ai_model >= 0 && anim_ai_model < 5) ? anim_ai_model : 0;
    return &LLM_THEMES[idx];
}

// 1. Unified Logarithmic Whirlpool / Spiral across all rings simultaneously:
//    - Center (0): Swirling vortex core singularity (voice-reactive model flare + REC indicator)
//    - Ring 0 (1..6): Inner eye arms (radius 1.0, model primary)
//    - Ring 1 (7..18): Middle vortex arms (radius 2.0, model accent)
//    - Ring 2 (19..42): Outer galaxy spiral arms (radius 3.2, model harmonic blend)
//    - Ring 3 (43..58): Accretion spiral outer rim (radius 4.4, model accent)
static void render_vortex(uint32_t tick, uint8_t mic_level_pct) {
    float voice_gain = (float)mic_level_pct / 100.0f;
    if (voice_gain > 1.0f) voice_gain = 1.0f;

    const llm_led_theme_t *th = get_active_llm_theme();

    // Center LED (index 0) - pulsating RED recording indicator with model flare on vocal peaks!
    float c_wave = (sinf((float)tick * 0.20f) + 1.0f) * 0.5f;
    pixels_r[0] = (uint8_t)(160 + 95.0f * (c_wave * 0.4f + voice_gain * 0.6f));
    pixels_g[0] = (uint8_t)(th->cg * 0.30f * voice_gain);
    pixels_b[0] = (uint8_t)(th->cb * 0.30f * voice_gain);

    // Angular rotation: accelerates dynamically when user speaks into mic!
    float rot_speed = 0.08f + 0.14f * voice_gain;
    float rot = (float)tick * rot_speed;

    // Ring 0 (1..6, 6 LEDs, radius 1.0): twist = 0.0 (model primary color)
    for (int i = 1; i <= 6; i++) {
        float theta = (float)(i - 1) * (2.0f * 3.14159265f / 6.0f);
        float diff = theta - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        pixels_r[i] = (uint8_t)(th->r1 * 0.08f + pwr * (th->r1 * (0.75f + 0.25f * voice_gain)));
        pixels_g[i] = (uint8_t)(th->g1 * 0.08f + pwr * (th->g1 * (0.75f + 0.25f * voice_gain)));
        pixels_b[i] = (uint8_t)(th->b1 * 0.08f + pwr * (th->b1 * (0.75f + 0.25f * voice_gain)));
    }

    // Ring 1 (7..18, 12 LEDs, radius 2.0): twist = 0.80 rad (~46 deg, model accent color)
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        float theta = (float)(i - 7) * (2.0f * 3.14159265f / 12.0f);
        float diff = theta - 0.80f - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        pixels_r[i] = (uint8_t)(th->r2 * 0.08f + pwr * (th->r2 * (0.75f + 0.25f * voice_gain)));
        pixels_g[i] = (uint8_t)(th->g2 * 0.08f + pwr * (th->g2 * (0.75f + 0.25f * voice_gain)));
        pixels_b[i] = (uint8_t)(th->b2 * 0.08f + pwr * (th->b2 * (0.75f + 0.25f * voice_gain)));
    }

    // Ring 2 (19..42, 24 LEDs, radius 3.2): twist = 1.65 rad (~95 deg, harmonic blend)
    float br_r = (th->r1 + th->r2) * 0.5f;
    float br_g = (th->g1 + th->g2) * 0.5f;
    float br_b = (th->b1 + th->b2) * 0.5f;
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        float theta = (float)(i - 19) * (2.0f * 3.14159265f / 24.0f);
        float diff = theta - 1.65f - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        pixels_r[i] = (uint8_t)(br_r * 0.05f + pwr * br_r);
        pixels_g[i] = (uint8_t)(br_g * 0.05f + pwr * br_g);
        pixels_b[i] = (uint8_t)(br_b * 0.05f + pwr * br_b);
    }

#if CELESTIAL_FOUR_RINGS
    // Ring 3 (43..58, 16 LEDs, radius 4.4): twist = 2.50 rad (~143 deg, model accent)
    for (int i = 43; i <= 58 && i < LED_RING_COUNT; i++) {
        float theta = (float)(i - 43) * (2.0f * 3.14159265f / 16.0f);
        float diff = theta - 2.50f - rot;
        float arm_prox = cosf(2.0f * diff);
        float norm = (arm_prox + 1.0f) * 0.5f;
        float pwr = norm * norm * norm;
        if (pwr >= 0.25f) {
            pixels_r[i] = (uint8_t)(pwr * th->r2);
            pixels_g[i] = (uint8_t)(pwr * th->g2);
            pixels_b[i] = (uint8_t)(pwr * th->b2);
        } else {
            pixels_r[i] = 0; pixels_g[i] = 0; pixels_b[i] = 0;
        }
    }
#endif
}

// 9. Concentric Data Reception: Rings pulse inward as distinct concentric circles
//    simulating sub-etha transmission packets cascading into the staff core with model-specific themes!
static void render_data_recv(uint32_t tick) {
    // If running Local Pi 5 Core (model 4): Special Quantum Matrix animation!
    if (anim_ai_model == 4) {
        // Center Core LED (0): Pulsing vibrant Raspberry Ruby core
        float core_breathe = 0.5f + 0.5f * sinf((float)tick * 0.45f);
        pixels_r[0] = (uint8_t)(255 * core_breathe);
        pixels_g[0] = (uint8_t)(15 * core_breathe);
        pixels_b[0] = (uint8_t)(80 * core_breathe);

        // Ring 0 (1..6, 6 LEDs): Fast clockwise spinning Emerald beam
        int r0_head = 1 + (int)((tick * 2) % 6);
        for (int i = 1; i <= 6; i++) {
            if (i == r0_head) {
                pixels_r[i] = 10; pixels_g[i] = 255; pixels_b[i] = 90;
            } else {
                pixels_r[i] = 0;  pixels_g[i] = 30;  pixels_b[i] = 15;
            }
        }

        // Ring 1 (7..18, 12 LEDs): Counter-clockwise spinning Cyber Cyan/Mint beam
        int r1_head = 7 + (int)((12 - (tick % 12)) % 12);
        for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
            if (i == r1_head) {
                pixels_r[i] = 0;  pixels_g[i] = 230; pixels_b[i] = 180;
            } else {
                pixels_r[i] = 0;  pixels_g[i] = 25;  pixels_b[i] = 20;
            }
        }

        // Ring 2 (19..42, 24 LEDs): Outward quantum pulse waves (emerald/mint)
        uint32_t r2_phase = tick % 12;
        float r2_glow = (r2_phase <= 4) ? (1.0f - (float)r2_phase * 0.22f) : 0.08f;
        for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
            pixels_r[i] = (uint8_t)(5   * r2_glow + 1);
            pixels_g[i] = (uint8_t)(255 * r2_glow + 6);
            pixels_b[i] = (uint8_t)(120 * r2_glow + 4);
        }
        return;
    }

    const llm_led_theme_t *th = get_active_llm_theme();
    uint32_t cycle = tick / 14;
    uint32_t phase = tick % 14;

    // Alternate packet beam colors using the LLM's primary and accent palette
    bool alt_color = (cycle % 2) == 1;
    uint8_t pr = alt_color ? th->r1 : th->r2;
    uint8_t pg = alt_color ? th->g1 : th->g2;
    uint8_t pb = alt_color ? th->b1 : th->b2;

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

    // Center Core LED: Flashes with model core flare when packet hits core
    if (c_int > 0.4f) {
        pixels_r[0] = (uint8_t)(th->cr * c_int);
        pixels_g[0] = (uint8_t)(th->cg * c_int);
        pixels_b[0] = (uint8_t)(th->cb * c_int);
    } else {
        pixels_r[0] = (uint8_t)(th->r1 * 0.15f + th->cr * 0.25f * c_int);
        pixels_g[0] = (uint8_t)(th->g1 * 0.15f + th->cg * 0.25f * c_int);
        pixels_b[0] = (uint8_t)(th->b1 * 0.15f + th->cb * 0.25f * c_int);
    }
}

// 10. Expanding Speech Ripples: Sonic shockwaves radiating outward from center to outer perimeter
//     simulating acoustic oracle speech broadcast with model-specific color themes
static void render_expand_speech(uint32_t tick) {
    uint32_t wave_phase = tick % 16;
    const llm_led_theme_t *th = get_active_llm_theme();

    uint8_t vr = th->r1;
    uint8_t vg = th->g1;
    uint8_t vb = th->b1;
    uint8_t cr = th->cr;
    uint8_t cg = th->cg;
    uint8_t cb = th->cb;

    // Center Core (0): Fires brightly at wave start, pulsing with model core color
    float c_wave = (wave_phase <= 3) ? (1.0f - (float)wave_phase * 0.25f) : 0.20f;
    pixels_r[0] = (uint8_t)(cr * c_wave);
    pixels_g[0] = (uint8_t)(cg * c_wave);
    pixels_b[0] = (uint8_t)(cb * c_wave);

    // Ring 0 (1..6, radius 1.0): Peak at phase 2..6
    float r0_wave = 0.05f;
    if (wave_phase >= 2 && wave_phase <= 6) {
        float d = (float)(wave_phase - 2) / 4.0f;
        r0_wave = 1.0f - d * 0.8f;
    }
    for (int i = 1; i <= 6; i++) {
        pixels_r[i] = (uint8_t)(vr * r0_wave + 6);
        pixels_g[i] = (uint8_t)(vg * r0_wave + 6);
        pixels_b[i] = (uint8_t)(vb * r0_wave + 6);
    }

    // Ring 1 (7..18, radius 2.0): Peak at phase 5..9
    float r1_wave = 0.05f;
    if (wave_phase >= 5 && wave_phase <= 9) {
        float d = (float)(wave_phase - 5) / 4.0f;
        r1_wave = 1.0f - d * 0.8f;
    }
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(vr * r1_wave + 4);
        pixels_g[i] = (uint8_t)(vg * r1_wave + 4);
        pixels_b[i] = (uint8_t)(vb * r1_wave + 4);
    }

    // Ring 2 (19..42, radius 3.2): Peak at phase 8..13
    float r2_wave = 0.05f;
    if (wave_phase >= 8 && wave_phase <= 13) {
        float d = (float)(wave_phase - 8) / 5.0f;
        r2_wave = 1.0f - d * 0.8f;
    }
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = (uint8_t)(vr * r2_wave + 2);
        pixels_g[i] = (uint8_t)(vg * r2_wave + 2);
        pixels_b[i] = (uint8_t)(vb * r2_wave + 2);
    }

#if CELESTIAL_FOUR_RINGS
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

// 11. Milky Way Galaxy: Sagittarius A* Core, swirling cosmic purple/cyan bulge, and golden Sol beacon
static void render_galaxy(uint32_t tick) {
    // Ring 0 (0..6, 7 LEDs): Sagittarius A* Galactic Supermassive Core!
    float core_flare = (sinf((float)tick * 0.18f) + 1.0f) * 0.5f;
    pixels_r[0] = (uint8_t)(230 + 25.0f * core_flare); // Blinding center flare
    pixels_g[0] = (uint8_t)(160 + 80.0f * core_flare);
    pixels_b[0] = 255;
    for (int i = 1; i <= 6; i++) {
        float f = (sinf((float)(tick * 5 + i * 60) * 0.06f) + 1.0f) * 0.5f;
        pixels_r[i] = (uint8_t)(140 + 70.0f * f); // Deep cosmic violet / magenta
        pixels_g[i] = (uint8_t)(20  + 30.0f * f);
        pixels_b[i] = (uint8_t)(200 + 55.0f * f);
    }

    // Ring 1 (7..18, 12 LEDs): Dense Inner Spiral Bulge & Bar
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int rel = i - 7;
        float arm = (sinf((float)(tick * 3 + rel * 60) * 0.08f) + 1.0f) * 0.5f;
        pixels_r[i] = (uint8_t)(30  + 60.0f * arm);  // Soft cosmic dust & star cloud
        pixels_g[i] = (uint8_t)(120 + 100.0f * arm); // Celestial cyan star clusters
        pixels_b[i] = (uint8_t)(200 + 55.0f * arm);
    }

    // Ring 2 (19..42, 24 LEDs): Outer Spiral Arms & Sol System (Orion Spur)
    int sol_idx = 19 + ((tick / 4) % 24);
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        if (i == sol_idx) {
            // Sol System Beacon: Golden Yellow star beacon pulsing on Orion Spur!
            bool pulse = ((tick / 2) % 3) == 0;
            if (pulse) {
                pixels_r[i] = 255; pixels_g[i] = 240; pixels_b[i] = 120;
            } else {
                pixels_r[i] = 255; pixels_g[i] = 180; pixels_b[i] = 20;
            }
        } else {
            // Interstellar velvet void with subtle drifting star glints
            if ((i * 5 + tick) % 13 == 0) {
                pixels_r[i] = 10; pixels_g[i] = 20; pixels_b[i] = 50;
            } else {
                pixels_r[i] = 1;  pixels_g[i] = 1;  pixels_b[i] = 6;
            }
        }
    }
}

// 12. Terra (Planet Earth): Geothermal core, revolving azure oceans & emerald continents, polar aurora
static void render_earth(uint32_t tick) {
    // Ring 0 (0..6, 7 LEDs): Molten Iron-Nickel Geothermal Core
    float core_pulse = (sinf((float)tick * 0.10f) + 1.0f) * 0.5f;
    for (int i = 0; i <= 6; i++) {
        pixels_r[i] = (uint8_t)(220 + 35.0f * core_pulse);
        pixels_g[i] = (uint8_t)(90  + 40.0f * core_pulse);
        pixels_b[i] = (uint8_t)(10  + 15.0f * core_pulse);
    }

    // Ring 1 (7..18, 12 LEDs): Rotating Continents, Oceans, and Cloud Caps
    int rot = (tick / 5) % 12;
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int pos = (i - 7 + rot) % 12;
        if (pos == 0 || pos == 1 || pos == 5 || pos == 6) {
            // Azure Blue Oceans (Pacific / Atlantic)
            pixels_r[i] = 0;   pixels_g[i] = 110; pixels_b[i] = 245;
        } else if (pos == 2 || pos == 3 || pos == 7 || pos == 8) {
            // Emerald & Jade Green Continents
            pixels_r[i] = 20;  pixels_g[i] = 220; pixels_b[i] = 50;
        } else if (pos == 4 || pos == 9) {
            // Golden Savannah / Desert Sand
            pixels_r[i] = 210; pixels_g[i] = 175; pixels_b[i] = 35;
        } else {
            // Polar Ice & Drifting White Clouds
            pixels_r[i] = 220; pixels_g[i] = 240; pixels_b[i] = 255;
        }
    }

    // Ring 2 (19..42, 24 LEDs): Upper Atmosphere & Dancing Aurora Borealis
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        int rel = i - 19;
        float aurora = (sinf((float)(tick * 4 + rel * 45) * 0.07f) + 1.0f) * 0.5f;
        pixels_r[i] = (uint8_t)(10  + 30.0f * aurora);
        pixels_g[i] = (uint8_t)(140 + 115.0f * aurora); // Ethereal emerald / cyan
        pixels_b[i] = (uint8_t)(90  + 120.0f * aurora);
    }
}

// 13. Halloween Spook: Flickering Jack-o'-Lantern pumpkin, haunted witch purple, toxic ectoplasm
static void render_halloween(uint32_t tick) {
    // Ring 0 (0..6, 7 LEDs): Jack-o'-Lantern Candle Flame Flicker
    uint8_t flicker = (uint8_t)((tick * 17) % 65);
    pixels_r[0] = 255;
    pixels_g[0] = 140 + flicker / 2;
    pixels_b[0] = 10;
    for (int i = 1; i <= 6; i++) {
        uint8_t f = (uint8_t)(((tick * 11) + i * 29) % 55);
        pixels_r[i] = 255;                // Pumpkin orange / fiery amber
        pixels_g[i] = (uint8_t)(60 + f);
        pixels_b[i] = 0;
    }

    // Ring 1 (7..18, 12 LEDs): Haunted Witch Cauldron & Amethyst Shadows
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int rel = i - 7;
        float wave = (sinf((float)(tick * 3 + rel * 60) * 0.08f) + 1.0f) * 0.5f;
        pixels_r[i] = (uint8_t)(160 + 90.0f * wave); // Witch purple / eerie amethyst
        pixels_g[i] = (uint8_t)(5   + 20.0f * wave);
        pixels_b[i] = (uint8_t)(180 + 75.0f * wave);
    }

    // Ring 2 (19..42, 24 LEDs): Creepy Toxic Ectoplasm & Floating Phantoms
    int ghost_idx = 19 + ((tick / 3) % 24);
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        if (i == ghost_idx || i == 19 + ((ghost_idx - 19 + 12) % 24)) {
            // Ghostly wisp: Creepy toxic neon ectoplasm green!
            pixels_r[i] = 30;  pixels_g[i] = 255; pixels_b[i] = 50;
        } else {
            // Shadowy haunted darkness with subtle toxic glow
            if ((i * 3 + tick) % 7 == 0) {
                pixels_r[i] = 5;  pixels_g[i] = 30;  pixels_b[i] = 10;
            } else {
                pixels_r[i] = 0;  pixels_g[i] = 0;   pixels_b[i] = 0;
            }
        }
    }
}

// 14. Celestial Chronometer: 3 concentric time rings with central pulsing stone
static uint8_t anim_clock_hours = 12;
static uint8_t anim_clock_minutes = 0;
static uint8_t anim_clock_seconds = 0;
static uint16_t anim_clock_millis = 0;

void leds_set_clock_time(uint8_t hours, uint8_t minutes, uint8_t seconds, uint16_t millis) {
    anim_clock_hours = hours;
    anim_clock_minutes = minutes;
    anim_clock_seconds = seconds;
    anim_clock_millis = millis;
}

static inline uint8_t add_sat_u8(uint8_t a, uint8_t b) {
    uint16_t sum = (uint16_t)a + (uint16_t)b;
    return (sum > 255) ? 255 : (uint8_t)sum;
}

static void render_clock(uint32_t tick) {
    (void)tick;
    // Ring 0 (Indices 0..6, 7 LEDs): Center Stone (Pendulum Heartbeat) & 10-Second Decan Accumulator
    // Center Stone (Index 0): 1 Hz rhythmic pendulum heartbeat pulse
    float sec_phase = (float)anim_clock_millis / 1000.0f;
    float pulse = 0.5f + 0.5f * sinf(sec_phase * 6.2831853f - 1.5707963f);
    bool minute_flare = (anim_clock_seconds == 0 && anim_clock_millis < 350);

    if (minute_flare) {
        // Supernova flare at top of minute!
        pixels_r[0] = 255; pixels_g[0] = 255; pixels_b[0] = 230;
    } else {
        // Warm glowing Amber / Topaz heartbeat
        pixels_r[0] = (uint8_t)(190 + 65.0f * pulse);
        pixels_g[0] = (uint8_t)(110 + 70.0f * pulse);
        pixels_b[0] = (uint8_t)(15  + 20.0f * pulse);
    }

    // Inner Ring (Indices 1..6, 6 LEDs): 10-Second Decans (0..59s)
    int active_decan = anim_clock_seconds / 10; // 0..5
    float decan_prog = (float)(anim_clock_seconds % 10) / 10.0f + (float)anim_clock_millis / 10000.0f;
    for (int i = 1; i <= 6; i++) {
        int dec_idx = i - 1;
        if (minute_flare) {
            // Outward ripple flash across inner ring
            pixels_r[i] = 240; pixels_g[i] = 200; pixels_b[i] = 255;
        } else if (dec_idx < active_decan) {
            // Full decan: Celestial Amethyst / Violet
            pixels_r[i] = 170; pixels_g[i] = 35;  pixels_b[i] = 220;
        } else if (dec_idx == active_decan) {
            // Advancing decan: Smooth accumulation glow
            pixels_r[i] = 40 + (uint8_t)(130.0f * decan_prog);
            pixels_g[i] = 10 + (uint8_t)(25.0f  * decan_prog);
            pixels_b[i] = 60 + (uint8_t)(160.0f * decan_prog);
        } else {
            // Unlit decan marker: Soft starlight placeholder
            pixels_r[i] = 3;   pixels_g[i] = 2;   pixels_b[i] = 8;
        }
    }

    // Ring 1 (Indices 7..18, 12 LEDs): 12 Clock Hours Dial
    // 12 LEDs = 12 hours (idx 7 = 12h, idx 8 = 1h, ... idx 18 = 11h)
    int h12 = anim_clock_hours % 12;
    int cur_h_led = 7 + h12;
    int next_h_led = 7 + ((h12 + 1) % 12);
    float m_fraction = (float)anim_clock_minutes / 60.0f;

    // Fill 12-hour dial with celestial constellation markers
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int h = i - 7;
        if (h == 0 || h == 3 || h == 6 || h == 9) {
            // Cardinal hours (12, 3, 6, 9 o'clock)
            pixels_r[i] = 8;  pixels_g[i] = 16; pixels_b[i] = 45;
        } else {
            // Minor hours
            pixels_r[i] = 3;  pixels_g[i] = 6;  pixels_b[i] = 20;
        }
    }

    // Blend Hour Hand: Radiant Solar Gold
    pixels_r[cur_h_led] = add_sat_u8(pixels_r[cur_h_led], (uint8_t)(255.0f * (1.0f - m_fraction * 0.65f)));
    pixels_g[cur_h_led] = add_sat_u8(pixels_g[cur_h_led], (uint8_t)(175.0f * (1.0f - m_fraction * 0.65f)));
    pixels_b[cur_h_led] = add_sat_u8(pixels_b[cur_h_led], (uint8_t)(20.0f  * (1.0f - m_fraction * 0.65f)));

    pixels_r[next_h_led] = add_sat_u8(pixels_r[next_h_led], (uint8_t)(255.0f * (m_fraction * 0.65f)));
    pixels_g[next_h_led] = add_sat_u8(pixels_g[next_h_led], (uint8_t)(175.0f * (m_fraction * 0.65f)));
    pixels_b[next_h_led] = add_sat_u8(pixels_b[next_h_led], (uint8_t)(20.0f  * (m_fraction * 0.65f)));

    // Ring 2 (Indices 19..42, 24 LEDs): Outer Perimeter Rim
    // Dual Hands: Emerald Minute Marker + Sweeping Second Comet
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        int rel = i - 19;
        if (rel % 6 == 0) {
            // Quadrant ticks (0, 15, 30, 45 min)
            pixels_r[i] = 4;  pixels_g[i] = 10; pixels_b[i] = 28;
        } else {
            pixels_r[i] = 1;  pixels_g[i] = 2;  pixels_b[i] = 8;
        }
    }

    // Minute Marker Hand (Emerald / Electric Cyan beacon)
    float min_pos = ((float)anim_clock_minutes * 24.0f / 60.0f) + ((float)anim_clock_seconds * 24.0f / 3600.0f);
    int m0 = 19 + ((int)min_pos % 24);
    int m1 = 19 + (((int)min_pos + 1) % 24);
    float m_sub = min_pos - (float)((int)min_pos);

    if (m0 < LED_RING_COUNT) {
        pixels_g[m0] = add_sat_u8(pixels_g[m0], (uint8_t)(240.0f * (1.0f - m_sub)));
        pixels_b[m0] = add_sat_u8(pixels_b[m0], (uint8_t)(180.0f * (1.0f - m_sub)));
    }
    if (m1 < LED_RING_COUNT) {
        pixels_g[m1] = add_sat_u8(pixels_g[m1], (uint8_t)(240.0f * m_sub));
        pixels_b[m1] = add_sat_u8(pixels_b[m1], (uint8_t)(180.0f * m_sub));
    }

    // Sweeping Second Comet (Diamond White head with electric azure fading tail)
    float sec_pos = ((float)anim_clock_seconds * 24.0f / 60.0f) + ((float)anim_clock_millis * 24.0f / 60000.0f);
    int s_head  = 19 + ((int)sec_pos % 24);
    int s_tail1 = 19 + (((int)sec_pos - 1 + 24) % 24);
    int s_tail2 = 19 + (((int)sec_pos - 2 + 24) % 24);
    int s_tail3 = 19 + (((int)sec_pos - 3 + 24) % 24);

    if (s_tail3 < LED_RING_COUNT) {
        pixels_r[s_tail3] = add_sat_u8(pixels_r[s_tail3], 5);
        pixels_g[s_tail3] = add_sat_u8(pixels_g[s_tail3], 15);
        pixels_b[s_tail3] = add_sat_u8(pixels_b[s_tail3], 60);
    }
    if (s_tail2 < LED_RING_COUNT) {
        pixels_r[s_tail2] = add_sat_u8(pixels_r[s_tail2], 20);
        pixels_g[s_tail2] = add_sat_u8(pixels_g[s_tail2], 60);
        pixels_b[s_tail2] = add_sat_u8(pixels_b[s_tail2], 170);
    }
    if (s_tail1 < LED_RING_COUNT) {
        pixels_r[s_tail1] = add_sat_u8(pixels_r[s_tail1], 70);
        pixels_g[s_tail1] = add_sat_u8(pixels_g[s_tail1], 160);
        pixels_b[s_tail1] = add_sat_u8(pixels_b[s_tail1], 255);
    }
    if (s_head < LED_RING_COUNT) {
        pixels_r[s_head] = add_sat_u8(pixels_r[s_head], 255);
        pixels_g[s_head] = add_sat_u8(pixels_g[s_head], 255);
        pixels_b[s_head] = add_sat_u8(pixels_b[s_head], 255);
    }

    // Tail slots beyond 43 (if present in four-ring mode)
    for (int i = 43; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 2;  pixels_g[i] = 4;  pixels_b[i] = 12;
    }
}

// 15. Aetheric Hourglass: Orientation-independent concentric ring accumulator
// Middle Ring = 1..12 lit LEDs (Hours count)
// Outer Ring = 0..24 lit LEDs (Minutes proportional liquid fill)
// Inner Ring = 0..6 lit LEDs (10s of seconds decans)
// Center Stone = 1 Hz pulsing pendulum heart
static void render_hourglass(uint32_t tick) {
    (void)tick;
    // 1. Center Stone (Index 0): 1 Hz rhythmic pendulum heartbeat pulse
    float sec_phase = (float)anim_clock_millis / 1000.0f;
    float pulse = 0.5f + 0.5f * sinf(sec_phase * 6.2831853f - 1.5707963f);
    bool minute_flare = (anim_clock_seconds == 0 && anim_clock_millis < 350);

    if (minute_flare) {
        pixels_r[0] = 255; pixels_g[0] = 255; pixels_b[0] = 240;
    } else {
        pixels_r[0] = (uint8_t)(190 + 65.0f * pulse);
        pixels_g[0] = (uint8_t)(110 + 70.0f * pulse);
        pixels_b[0] = (uint8_t)(15  + 20.0f * pulse);
    }

    // 2. Inner Ring (Indices 1..6, 6 LEDs): 10-Second Decans (0..59s)
    int num_decans = anim_clock_seconds / 10; // 0..5
    float decan_fraction = ((float)(anim_clock_seconds % 10) + (float)anim_clock_millis / 1000.0f) / 10.0f;
    for (int i = 1; i <= 6; i++) {
        int d = i - 1;
        if (minute_flare) {
            pixels_r[i] = 240; pixels_g[i] = 200; pixels_b[i] = 255;
        } else if (d < num_decans) {
            // Full decan: Celestial Amethyst / Violet
            pixels_r[i] = 170; pixels_g[i] = 35;  pixels_b[i] = 220;
        } else if (d == num_decans) {
            // Advancing decan: Gradual accumulation
            pixels_r[i] = 30 + (uint8_t)(140.0f * decan_fraction);
            pixels_g[i] = 5  + (uint8_t)(30.0f  * decan_fraction);
            pixels_b[i] = 50 + (uint8_t)(170.0f * decan_fraction);
        } else {
            pixels_r[i] = 3;   pixels_g[i] = 1;   pixels_b[i] = 8;
        }
    }

    // 3. Middle Ring (Indices 7..18, 12 LEDs): Hours Count (1 to 12 lit LEDs)
    int h = anim_clock_hours % 12;
    if (h == 0) h = 12;
    float m_prog = (float)anim_clock_minutes / 60.0f;

    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        int step = i - 7; // 0..11
        if (step < h) {
            // Filled Hour LED: Radiant Solar Gold
            pixels_r[i] = 255;
            pixels_g[i] = 175;
            pixels_b[i] = 20;
        } else if (step == (h % 12)) {
            // Frontier Hour LED: Warming up proportionally to minutes (0..59m)
            pixels_r[i] = 3  + (uint8_t)(252.0f * (m_prog * 0.70f));
            pixels_g[i] = 6  + (uint8_t)(169.0f * (m_prog * 0.70f));
            pixels_b[i] = 18 + (uint8_t)(2.0f   * (m_prog * 0.70f));
        } else {
            // Unfilled: Dim velvet sapphire
            pixels_r[i] = 3;
            pixels_g[i] = 6;
            pixels_b[i] = 18;
        }
    }

    // 4. Outer Ring (Indices 19..42, 24 LEDs): Minutes Liquid-Fill Reservoir (0..24 LEDs)
    float m_exact = (float)anim_clock_minutes * 24.0f / 60.0f + (float)anim_clock_seconds * 24.0f / 3600.0f;
    int full_m_leds = (int)m_exact;
    float m_sub = m_exact - (float)full_m_leds;

    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        int rel = i - 19; // 0..23
        if (rel < full_m_leds) {
            // Filled Minutes Reservoir: Liquid Emerald / Cyan
            pixels_r[i] = 0;
            pixels_g[i] = 220;
            pixels_b[i] = 160;
        } else if (rel == full_m_leds) {
            // Advancing liquid meniscus
            uint8_t g_val = (uint8_t)(220.0f * m_sub);
            uint8_t b_val = (uint8_t)(160.0f * m_sub);
            // Glistening meniscus droplet
            pixels_r[i] = (uint8_t)(40.0f * pulse);
            pixels_g[i] = add_sat_u8(g_val, (uint8_t)(35.0f * pulse));
            pixels_b[i] = add_sat_u8(b_val, (uint8_t)(60.0f * pulse));
        } else {
            // Unfilled reservoir
            pixels_r[i] = 1;
            pixels_g[i] = 2;
            pixels_b[i] = 8;
        }
    }

    // Tail slots beyond 43 (if present in four-ring mode)
    for (int i = 43; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 2;  pixels_g[i] = 4;  pixels_b[i] = 12;
    }
}

// 16. Chroma Prismatic: Full spectral color-encoded clock (Orientation-free)
// Middle Ring (12 LEDs) = 12 Spectral Hues representing 12 Hours
// Outer Ring (24 LEDs)  = 6 Decade Hues representing Minutes (fills proportionally 0..24 LEDs)
// Inner Ring (6 LEDs)   = 6 Prismatic Gems representing 10s of seconds (0..6 lit LEDs)
// Center Stone (LED 0)  = 1 Hz Chroma Heart pulsing in the 10-second decade hue
static void render_prismatic(uint32_t tick) {
    (void)tick;
    // 12 Spectral Hues for 12 Hours (idx 0..11)
    static const uint8_t HOUR_HUES[12][3] = {
        { 255,   0, 160 }, // 12h: Royal Magenta
        { 255,  15,  30 }, // 1h:  Ruby Red
        { 255,  60,   0 }, // 2h:  Scarlet Flame
        { 255, 115,   0 }, // 3h:  Solar Orange
        { 255, 175,   0 }, // 4h:  Amber Topaz
        { 240, 220,  15 }, // 5h:  Warm Gold
        { 120, 255,  20 }, // 6h:  Lime Chrysolite
        {   0, 255,  70 }, // 7h:  Emerald Green
        {   0, 230, 170 }, // 8h:  Aquamarine Teal
        {   0, 175, 255 }, // 9h:  Electric Azure
        {  25,  75, 255 }, // 10h: Cobalt Sapphire
        { 160,  20, 255 }  // 11h: Amethyst Purple
    };

    // 6 Decade Hues for Minutes & Seconds (0..5)
    static const uint8_t DECADE_HUES[6][3] = {
        {   0, 220, 255 }, // 0: Ice Cyan (00..09)
        {  20, 255,  80 }, // 1: Jade Emerald (10..19)
        { 255, 200,  15 }, // 2: Solar Gold (20..29)
        { 255,  95,   0 }, // 3: Plasma Orange (30..39)
        { 255,  30,  70 }, // 4: Coral Ruby (40..49)
        { 190,  20, 255 }  // 5: Cosmic Amethyst (50..59)
    };

    float sec_phase = (float)anim_clock_millis / 1000.0f;
    float pulse = 0.5f + 0.5f * sinf(sec_phase * 6.2831853f - 1.5707963f);
    bool minute_flare = (anim_clock_seconds == 0 && anim_clock_millis < 350);

    int s_decan = anim_clock_seconds / 10; if (s_decan > 5) s_decan = 5;
    int m_decan = anim_clock_minutes / 10; if (m_decan > 5) m_decan = 5;
    int h12 = anim_clock_hours % 12;

    // 1. Center Stone (LED 0): 1 Hz Heartbeat in 10-Second Decade Hue
    if (minute_flare) {
        pixels_r[0] = 255; pixels_g[0] = 255; pixels_b[0] = 240;
    } else {
        const uint8_t *dh = DECADE_HUES[s_decan];
        pixels_r[0] = (uint8_t)(dh[0] * (0.45f + 0.55f * pulse));
        pixels_g[0] = (uint8_t)(dh[1] * (0.45f + 0.55f * pulse));
        pixels_b[0] = (uint8_t)(dh[2] * (0.45f + 0.55f * pulse));
    }

    // 2. Inner Ring (LEDs 1..6): 10s of Seconds Decans (0..6 lit LEDs)
    float decan_fraction = ((float)(anim_clock_seconds % 10) + (float)anim_clock_millis / 1000.0f) / 10.0f;
    for (int i = 1; i <= 6; i++) {
        int d = i - 1; // 0..5
        const uint8_t *ch = DECADE_HUES[d];
        if (minute_flare) {
            pixels_r[i] = 240; pixels_g[i] = 220; pixels_b[i] = 255;
        } else if (d < s_decan) {
            pixels_r[i] = ch[0]; pixels_g[i] = ch[1]; pixels_b[i] = ch[2];
        } else if (d == s_decan) {
            pixels_r[i] = (uint8_t)(ch[0] * decan_fraction);
            pixels_g[i] = (uint8_t)(ch[1] * decan_fraction);
            pixels_b[i] = (uint8_t)(ch[2] * decan_fraction);
        } else {
            pixels_r[i] = (uint8_t)(ch[0] / 24);
            pixels_g[i] = (uint8_t)(ch[1] / 24);
            pixels_b[i] = (uint8_t)(ch[2] / 24);
        }
    }

    // 3. Middle Ring (LEDs 7..18, 12 LEDs): Uniform Hour Spectral Wash
    const uint8_t *hh = HOUR_HUES[h12];
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = hh[0];
        pixels_g[i] = hh[1];
        pixels_b[i] = hh[2];
    }

    // 4. Outer Ring (LEDs 19..42, 24 LEDs): Minute Reservoir with Decade Hue
    const uint8_t *mh = DECADE_HUES[m_decan];
    float m_exact = (float)anim_clock_minutes * 24.0f / 60.0f + (float)anim_clock_seconds * 24.0f / 3600.0f;
    int full_m_leds = (int)m_exact;
    float m_sub = m_exact - (float)full_m_leds;

    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        int rel = i - 19; // 0..23
        if (rel < full_m_leds) {
            pixels_r[i] = mh[0];
            pixels_g[i] = mh[1];
            pixels_b[i] = mh[2];
        } else if (rel == full_m_leds) {
            pixels_r[i] = (uint8_t)(mh[0] * m_sub);
            pixels_g[i] = (uint8_t)(mh[1] * m_sub);
            pixels_b[i] = (uint8_t)(mh[2] * m_sub);
        } else {
            pixels_r[i] = (uint8_t)(mh[0] / 30);
            pixels_g[i] = (uint8_t)(mh[1] / 30);
            pixels_b[i] = (uint8_t)(mh[2] / 30);
        }
    }

    for (int i = 43; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 2; pixels_g[i] = 4; pixels_b[i] = 12;
    }
}

// 17. Celestial Orrery: Kinematic Triple Orbital Clock (Orientation-free)
// Center Stone (LED 0) = The Sun / Core Star with coronal flares
// Inner Ring (1..6)    = Harmonic Hour Resonance Field
// Middle Ring (7..18)  = Planetary Orbit for Minutes
// Outer Ring (19..42)  = Sweeping Second Orbital Comet (1 rev / 60s)
static void render_orrery(uint32_t tick) {
    (void)tick;
    float sec_phase = ((float)anim_clock_seconds * 1000.0f + (float)anim_clock_millis) / 60000.0f;

    // 1. Center Stone (LED 0): The Sun
    float sun_flare = 0.5f + 0.5f * sinf((float)anim_clock_millis * 0.012f);
    pixels_r[0] = 255;
    pixels_g[0] = (uint8_t)(185 + 60.0f * sun_flare);
    pixels_b[0] = (uint8_t)(25  + 35.0f * sun_flare);

    // 2. Inner Ring (1..6, 6 LEDs): Hour Resonance Field
    int h12 = anim_clock_hours % 12; if (h12 == 0) h12 = 12;
    int h_active = (h12 > 6) ? (h12 - 6) : h12;
    bool is_pm = (anim_clock_hours >= 12);

    for (int i = 1; i <= 6; i++) {
        int d = i - 1;
        if (d < h_active) {
            if (is_pm) {
                // Royal Violet in afternoon/evening
                pixels_r[i] = 180; pixels_g[i] = 30; pixels_b[i] = 240;
            } else {
                // Solar Gold in morning
                pixels_r[i] = 255; pixels_g[i] = 190; pixels_b[i] = 20;
            }
        } else {
            pixels_r[i] = 2; pixels_g[i] = 4; pixels_b[i] = 12;
        }
    }

    // 3. Middle Ring (7..18, 12 LEDs): Orbiting Minute Planet
    float m_pos = ((float)anim_clock_minutes * 12.0f / 60.0f) + ((float)anim_clock_seconds * 12.0f / 3600.0f);
    int p_lead = 7 + ((int)m_pos % 12);
    int p_next = 7 + (((int)m_pos + 1) % 12);
    float p_frac = m_pos - (float)((int)m_pos);

    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = 3; pixels_g[i] = 8; pixels_b[i] = 22; // Orbit path
    }
    if (p_lead < LED_RING_COUNT) {
        pixels_r[p_lead] = add_sat_u8(pixels_r[p_lead], (uint8_t)(40.0f * (1.0f - p_frac)));
        pixels_g[p_lead] = add_sat_u8(pixels_g[p_lead], (uint8_t)(220.0f * (1.0f - p_frac)));
        pixels_b[p_lead] = add_sat_u8(pixels_b[p_lead], (uint8_t)(255.0f * (1.0f - p_frac)));
    }
    if (p_next < LED_RING_COUNT) {
        pixels_r[p_next] = add_sat_u8(pixels_r[p_next], (uint8_t)(40.0f * p_frac));
        pixels_g[p_next] = add_sat_u8(pixels_g[p_next], (uint8_t)(220.0f * p_frac));
        pixels_b[p_next] = add_sat_u8(pixels_b[p_next], (uint8_t)(255.0f * p_frac));
    }

    // 4. Outer Ring (19..42, 24 LEDs): Sweeping Second Orbital Comet (1 turn / 60s)
    float s_orbit = sec_phase * 24.0f;
    int s_head = 19 + ((int)s_orbit % 24);
    int s_t1   = 19 + (((int)s_orbit - 1 + 24) % 24);
    int s_t2   = 19 + (((int)s_orbit - 2 + 24) % 24);
    int s_t3   = 19 + (((int)s_orbit - 3 + 24) % 24);

    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        pixels_r[i] = 1; pixels_g[i] = 3; pixels_b[i] = 10;
    }
    if (s_t3 < LED_RING_COUNT) {
        pixels_r[s_t3] = add_sat_u8(pixels_r[s_t3], 15);
        pixels_g[s_t3] = add_sat_u8(pixels_g[s_t3], 40);
        pixels_b[s_t3] = add_sat_u8(pixels_b[s_t3], 90);
    }
    if (s_t2 < LED_RING_COUNT) {
        pixels_r[s_t2] = add_sat_u8(pixels_r[s_t2], 40);
        pixels_g[s_t2] = add_sat_u8(pixels_g[s_t2], 110);
        pixels_b[s_t2] = add_sat_u8(pixels_b[s_t2], 200);
    }
    if (s_t1 < LED_RING_COUNT) {
        pixels_r[s_t1] = add_sat_u8(pixels_r[s_t1], 100);
        pixels_g[s_t1] = add_sat_u8(pixels_g[s_t1], 200);
        pixels_b[s_t1] = add_sat_u8(pixels_b[s_t1], 255);
    }
    if (s_head < LED_RING_COUNT) {
        pixels_r[s_head] = 255;
        pixels_g[s_head] = 255;
        pixels_b[s_head] = 255;
    }

    for (int i = 43; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 2; pixels_g[i] = 4; pixels_b[i] = 12;
    }
}

// 18. Ouroboros Snake (SPELL_ANIM_SPIRAL_SNAKE):
// 2D continuous serpent spiraling through rings (24 -> 12 -> 6 -> 0 -> 24).
// The mythical cosmic snake winds around the outer boundary (24 LEDs, 19..42),
// transfers seamlessly at 0 degrees into the middle mantle (12 LEDs, 7..18),
// coils down into the inner ring (6 LEDs, 1..6), strikes the center stone (0)
// with a radiant gold-white nova flare, and erupts back outward to the perimeter!
static void render_spiral_snake(uint32_t tick) {
    static const uint8_t ouroboros_path[61] = {
        // Outer ring CW (24)
        19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
        // Middle ring CW (12)
        7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
        // Inner ring CW (6)
        1, 2, 3, 4, 5, 6,
        // Center Core Gem (1)
        0,
        // Erupting back outward: Inner ring CCW (6)
        6, 5, 4, 3, 2, 1,
        // Middle ring CCW (12)
        18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7
    };
    const int path_len = 61;
    const int snake_len = 12;
    int head_pos = (int)(tick % path_len);

    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 1;
        pixels_g[i] = 3;
        pixels_b[i] = 10;
    }

    for (int seg = snake_len - 1; seg >= 0; seg--) {
        int p_idx = (head_pos - seg + path_len) % path_len;
        uint8_t led_idx = ouroboros_path[p_idx];
        if (led_idx >= LED_RING_COUNT) continue;

        if (seg == 0) {
            pixels_r[led_idx] = 255;
            pixels_g[led_idx] = 245;
            pixels_b[led_idx] = 200;

            if (led_idx == 0) {
                for (int c = 1; c <= 6 && c < LED_RING_COUNT; c++) {
                    pixels_r[c] = add_sat_u8(pixels_r[c], 180);
                    pixels_g[c] = add_sat_u8(pixels_g[c], 120);
                    pixels_b[c] = add_sat_u8(pixels_b[c], 20);
                }
            }
        } else {
            float fade = 1.0f - ((float)seg / (float)snake_len);
            float seg_hue = (float)((tick * 5 + seg * 20) % 360);
            uint8_t r_seg, g_seg, b_seg;
            hsv_to_rgb((uint16_t)seg_hue, 240, (uint8_t)(255.0f * fade), &r_seg, &g_seg, &b_seg);

            pixels_r[led_idx] = add_sat_u8(pixels_r[led_idx], r_seg);
            pixels_g[led_idx] = add_sat_u8(pixels_g[led_idx], g_seg);
            pixels_b[led_idx] = add_sat_u8(pixels_b[led_idx], b_seg);
        }
    }
}

// 19. Concentric Ripple / Shockwave (SPELL_ANIM_RADIAL_WAVE):
// Circular wavefronts born in the center core crystal (LED 0, r=0.0)
// propagating outward across Ring 0 (r=1.0), Ring 1 (r=2.0), and Ring 2 (r=3.2).
// Implements continuous wave physics with inverse-distance damping (1/r)
// and smooth cosine wavefront dispersion.
static void render_radial_wave(uint32_t tick) {
    float t = (float)tick * 0.15f;
    const float ring_r[4] = { 0.0f, 1.0f, 2.0f, 3.2f };

    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 2;
        pixels_g[i] = 4;
        pixels_b[i] = 12;
    }

    for (int layer = 0; layer < 4; layer++) {
        float r = ring_r[layer];
        float wave_val = 0.0f;

        for (int p = 0; p < 2; p++) {
            float wave_front = fmodf(t + (float)p * 2.2f, 4.4f);
            float dist = fabsf(r - wave_front);
            if (dist < 0.75f) {
                float amp = 0.5f + 0.5f * cosf(dist * 3.14159f / 0.75f);
                float damping = 1.0f / (1.0f + 0.35f * r);
                wave_val += amp * damping;
            }
        }
        if (wave_val > 1.0f) wave_val = 1.0f;

        if (wave_val > 0.02f) {
            uint8_t wr = (uint8_t)(wave_val * (layer == 0 ? 255.0f : 40.0f));
            uint8_t wg = (uint8_t)(wave_val * (layer <= 1 ? 240.0f : 160.0f));
            uint8_t wb = (uint8_t)(wave_val * 255.0f);

            if (layer == 0) {
                pixels_r[0] = add_sat_u8(pixels_r[0], wr);
                pixels_g[0] = add_sat_u8(pixels_g[0], wg);
                pixels_b[0] = add_sat_u8(pixels_b[0], wb);
            } else if (layer == 1) {
                for (int i = 1; i <= 6 && i < LED_RING_COUNT; i++) {
                    pixels_r[i] = add_sat_u8(pixels_r[i], wr);
                    pixels_g[i] = add_sat_u8(pixels_g[i], wg);
                    pixels_b[i] = add_sat_u8(pixels_b[i], wb);
                }
            } else if (layer == 2) {
                for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
                    pixels_r[i] = add_sat_u8(pixels_r[i], wr);
                    pixels_g[i] = add_sat_u8(pixels_g[i], wg);
                    pixels_b[i] = add_sat_u8(pixels_b[i], wb);
                }
            } else if (layer == 3) {
                for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
                    pixels_r[i] = add_sat_u8(pixels_r[i], wr);
                    pixels_g[i] = add_sat_u8(pixels_g[i], wg);
                    pixels_b[i] = add_sat_u8(pixels_b[i], wb);
                }
            }
        }
    }
}

// 20. Quantum Waveform Collision (SPELL_ANIM_COLLIDING_WAVES):
// Two counter-propagating wave crests race around the rings in opposite directions:
// Wave 1: Clockwise (Azure Cyan, theta = omega * t)
// Wave 2: Counter-Clockwise (Neon Magenta / Crimson, theta = -omega * t)
// They collide at polar angles theta = 0 deg and theta = 180 deg.
// At collision nodes, constructive wave interference creates a blinding
// pure white shockwave flash, which resonates into the central crystal gem!
static void render_colliding_waves(uint32_t tick) {
    float theta_cw = fmodf((float)tick * 5.0f, 360.0f);
    float theta_ccw = fmodf(360.0f - theta_cw, 360.0f);

    float dist_node0 = fminf(theta_cw, 360.0f - theta_cw);
    float dist_node180 = fabsf(theta_cw - 180.0f);
    float min_collision_dist = fminf(dist_node0, dist_node180);

    float flash_factor = 0.0f;
    if (min_collision_dist < 18.0f) {
        flash_factor = 1.0f - (min_collision_dist / 18.0f);
        flash_factor = flash_factor * flash_factor;
    }

    for (int i = 0; i < LED_RING_COUNT; i++) {
        pixels_r[i] = 1;
        pixels_g[i] = 2;
        pixels_b[i] = 8;
    }

    if (flash_factor > 0.05f) {
        pixels_r[0] = (uint8_t)(255.0f * flash_factor);
        pixels_g[0] = (uint8_t)(255.0f * flash_factor);
        pixels_b[0] = (uint8_t)(255.0f * flash_factor);
    } else {
        pixels_r[0] = 5; pixels_g[0] = 8; pixels_b[0] = 24;
    }

    // Ring 0 (1..6)
    for (int i = 1; i <= 6 && i < LED_RING_COUNT; i++) {
        float angle = (float)(i - 1) * 60.0f;
        float d_cw = fabsf(angle - theta_cw); if (d_cw > 180.0f) d_cw = 360.0f - d_cw;
        float d_ccw = fabsf(angle - theta_ccw); if (d_ccw > 180.0f) d_ccw = 360.0f - d_ccw;

        float amp_cw = (d_cw < 45.0f) ? (1.0f - d_cw / 45.0f) : 0.0f;
        float amp_ccw = (d_ccw < 45.0f) ? (1.0f - d_ccw / 45.0f) : 0.0f;

        uint8_t r = (uint8_t)(amp_ccw * 255.0f);
        uint8_t g = (uint8_t)(amp_cw * 200.0f);
        uint8_t b = (uint8_t)(amp_cw * 255.0f + amp_ccw * 120.0f);

        if (flash_factor > 0.0f && (i == 1 || i == 4)) {
            r = add_sat_u8(r, (uint8_t)(255.0f * flash_factor));
            g = add_sat_u8(g, (uint8_t)(255.0f * flash_factor));
            b = add_sat_u8(b, (uint8_t)(255.0f * flash_factor));
        }

        pixels_r[i] = add_sat_u8(pixels_r[i], r);
        pixels_g[i] = add_sat_u8(pixels_g[i], g);
        pixels_b[i] = add_sat_u8(pixels_b[i], b);
    }

    // Ring 1 (7..18)
    for (int i = 7; i <= 18 && i < LED_RING_COUNT; i++) {
        float angle = (float)(i - 7) * 30.0f;
        float d_cw = fabsf(angle - theta_cw); if (d_cw > 180.0f) d_cw = 360.0f - d_cw;
        float d_ccw = fabsf(angle - theta_ccw); if (d_ccw > 180.0f) d_ccw = 360.0f - d_ccw;

        float amp_cw = (d_cw < 40.0f) ? (1.0f - d_cw / 40.0f) : 0.0f;
        float amp_ccw = (d_ccw < 40.0f) ? (1.0f - d_ccw / 40.0f) : 0.0f;

        uint8_t r = (uint8_t)(amp_ccw * 255.0f);
        uint8_t g = (uint8_t)(amp_cw * 220.0f);
        uint8_t b = (uint8_t)(amp_cw * 255.0f + amp_ccw * 80.0f);

        if (flash_factor > 0.0f && (i == 7 || i == 13)) {
            r = add_sat_u8(r, (uint8_t)(255.0f * flash_factor));
            g = add_sat_u8(g, (uint8_t)(255.0f * flash_factor));
            b = add_sat_u8(b, (uint8_t)(255.0f * flash_factor));
        }

        pixels_r[i] = add_sat_u8(pixels_r[i], r);
        pixels_g[i] = add_sat_u8(pixels_g[i], g);
        pixels_b[i] = add_sat_u8(pixels_b[i], b);
    }

    // Ring 2 (19..42)
    for (int i = 19; i <= 42 && i < LED_RING_COUNT; i++) {
        float angle = (float)(i - 19) * 15.0f;
        float d_cw = fabsf(angle - theta_cw); if (d_cw > 180.0f) d_cw = 360.0f - d_cw;
        float d_ccw = fabsf(angle - theta_ccw); if (d_ccw > 180.0f) d_ccw = 360.0f - d_ccw;

        float amp_cw = (d_cw < 30.0f) ? (1.0f - d_cw / 30.0f) : 0.0f;
        float amp_ccw = (d_ccw < 30.0f) ? (1.0f - d_ccw / 30.0f) : 0.0f;

        uint8_t r = (uint8_t)(amp_ccw * 255.0f);
        uint8_t g = (uint8_t)(amp_cw * 210.0f);
        uint8_t b = (uint8_t)(amp_cw * 255.0f + amp_ccw * 100.0f);

        if (flash_factor > 0.0f) {
            float node_dist = fminf(fabsf(angle), fabsf(angle - 180.0f));
            if (node_dist > 180.0f) node_dist = 360.0f - node_dist;
            if (node_dist < 20.0f) {
                float local_flash = flash_factor * (1.0f - node_dist / 20.0f);
                r = add_sat_u8(r, (uint8_t)(255.0f * local_flash));
                g = add_sat_u8(g, (uint8_t)(255.0f * local_flash));
                b = add_sat_u8(b, (uint8_t)(255.0f * local_flash));
            }
        }

        pixels_r[i] = add_sat_u8(pixels_r[i], r);
        pixels_g[i] = add_sat_u8(pixels_g[i], g);
        pixels_b[i] = add_sat_u8(pixels_b[i], b);
    }
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
            case LED_ANIM_GALAXY:
                render_galaxy(anim_tick);
                break;
            case LED_ANIM_EARTH:
                render_earth(anim_tick);
                break;
            case LED_ANIM_HALLOWEEN:
                render_halloween(anim_tick);
                break;
            case LED_ANIM_CLOCK:
                render_clock(anim_tick);
                break;
            case LED_ANIM_HOURGLASS:
                render_hourglass(anim_tick);
                break;
            case LED_ANIM_PRISMATIC:
                render_prismatic(anim_tick);
                break;
            case LED_ANIM_ORRERY:
                render_orrery(anim_tick);
                break;
            case LED_ANIM_SPIRAL_SNAKE:
                render_spiral_snake(anim_tick);
                break;
            case LED_ANIM_RADIAL_WAVE:
                render_radial_wave(anim_tick);
                break;
            case LED_ANIM_COLLIDING_WAVES:
                render_colliding_waves(anim_tick);
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
