#include "leds.h"
#include "led_patterns.h"
#include "audio.h"
#include "config.h"
#include "ws2812_driver.h"
#include "hardware/pio.h"
#include "pico/time.h"

static PIO led_pio = pio0;
static uint led_sm = 1;

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

static uint32_t led_tx_words[LED_RING_COUNT];
#if CELESTIAL_FOUR_RINGS
static absolute_time_t led_boot_test_started;
static int led_boot_test_phase = -1;
#endif

const led_color_t RAINBOW_COLORS[RAINBOW_COLOR_COUNT] = {
    {   0, 220, 255, "CYAN" },      // 0. Cosmic Cyan (Aether Core)
    {   0,  60, 255, "AZURE" },     // 1. Deep Space Azure Blue
    { 180,   0, 255, "VIOLET" },    // 2. Nebula Violet / Amethyst
    { 255,  15, 140, "MAGENTA" },   // 3. Cosmic Magenta / Rose
    { 255,   0,   0, "CRIMSON" },   // 4. Solar Flare Crimson
    { 255,  85,   0, "AMBER" },     // 5. Sunburst Amber / Orange
    { 255, 200,  15, "GOLD" },      // 6. Radiant Celestial Gold
    {   0, 255,  60, "EMERALD" },   // 7. Matrix Emerald Green
    { 255, 255, 255, "STARLIGHT" }, // 8. Pure Starlight White
    {   0,   0,   0, "SPECTRUM" }   // 9. Dynamic Chromatic Breath (Smooth Rainbow)
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
    "COLLIDING WAVES",
    "ARCHIMEDES SPIRAL",
    "SACRED POLYGON",
    "DOOM HELLFIRE",
    "TETRIS ARCADE",
    "ZELDA TRIFORCE",
    "STAR WARS DUEL",
    "GHOSTBUSTERS",
    "MORROWIND ASH",
    "SPOOKY SKELETONS",
    "DAFT PUNK VISOR",
    "EBAY AUCTION",
    "HARRY POTTER",
    "POKEMON POKEBALL",
    "SAILOR MOON"
};

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t r_scaled = (uint32_t)(r * master_brightness) / 100;
    uint32_t g_scaled = (uint32_t)(g * master_brightness) / 100;
    uint32_t b_scaled = (uint32_t)(b * master_brightness) / 100;
    return ((uint32_t)g_scaled << 16) | ((uint32_t)r_scaled << 8) | (uint32_t)b_scaled;
}

static void show_pixels(void) {
    for (int i = 0; i < LED_RING_COUNT; ++i) {
        led_tx_words[i] = rgb_to_grb(pixels_r[i], pixels_g[i], pixels_b[i]) << 8u;
    }
    ws2812_driver_transmit_blocking(led_tx_words, LED_RING_COUNT);
}

void leds_init(void) {
    ws2812_driver_init(led_pio, led_sm, PIN_WS2812_DATA, 800000.0f);

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

int leds_get_rainbow_index(void) {
    return base_color_idx;
}

int leds_get_rainbow_count(void) {
    return RAINBOW_COLOR_COUNT;
}

const char* leds_get_rainbow_name(int color_idx) {
    if (color_idx < 0) color_idx = 0;
    return RAINBOW_COLORS[color_idx % RAINBOW_COLOR_COUNT].name;
}

void leds_set_brightness(uint8_t brightness_pct) {
    if (brightness_pct > LED_BRIGHTNESS_LIMIT) brightness_pct = LED_BRIGHTNESS_LIMIT;
    master_brightness = brightness_pct;
}

void leds_set_clock_time(uint8_t hours, uint8_t minutes, uint8_t seconds, uint16_t millis) {
    led_patterns_set_clock_time(hours, minutes, seconds, millis);
}

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

    int current_mode = selected_anim_idx;
    bool music_active = audio_is_chiptune_playing() && (current_mode == LED_ANIM_RAINBOW);
    if (easter_egg_active) {
        // Hyperspeed 67 easter egg rainbow vortex!
        uint16_t fast_hue = (anim_tick * 16) % 360;
        for (int i = 0; i < LED_RING_COUNT; i++) {
            uint16_t led_hue = (fast_hue + (i * 360 / LED_RING_COUNT)) % 360;
            led_patterns_hsv_to_rgb(led_hue, 255, 255, &pixels_r[i], &pixels_g[i], &pixels_b[i]);
        }
    } else if (music_active) {
        // Active chiptune synchronization across all rings
        led_patterns_render_chiptune_sync(anim_tick, pixels_r, pixels_g, pixels_b);
    } else {
        led_patterns_render(current_mode, anim_tick, base_color_idx, anim_ai_model,
                            mic_level_pct, pixels_r, pixels_g, pixels_b);
    }

#if CELESTIAL_FOUR_RINGS && CELESTIAL_RING_EXPERIMENT
    if (!easter_egg_active) {
        led_patterns_render_spare_ring(anim_tick, current_mode, mic_level_pct, music_active,
                                       pixels_r, pixels_g, pixels_b);
    }
#endif
    show_pixels();
}
