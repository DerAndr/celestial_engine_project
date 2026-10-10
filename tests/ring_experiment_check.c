/* One native check of main/diagnostic four-ring frames and actual LED renderers. */
#include "firmware_harness.c"
#undef printf

static int check_transfer(void) {
    dma_channel_config c = test_dma_configs[test_dma_last_channel];
    if (test_dma_word_count != 59 || c.size != DMA_SIZE_32 ||
        !c.read_increment || c.write_increment || !c.high_priority ||
        c.dreq != pio_get_dreq(pio0, 1, true) || test_last_sleep_us < 330) return 1;
    for (int i = 0; i < 59; ++i) {
        if (test_dma_words[i] != (rgb_to_grb(pixels_r[i], pixels_g[i], pixels_b[i]) << 8u)) return 1;
    }
    return 0;
}

static int check_frame(void) {
    uint before = accepted_pixels;
    uint waits = test_dma_wait_count;
    test_now += 31000;
    leds_update(false, true, active_anim_mode, 0);
    if (accepted_pixels - before != 59 || test_dma_wait_count != waits + 1 || check_transfer()) return 1;
    return 0;
}

static int check_dark_backgrounds(void) {
    int failures = 0;
    const int modes[] = {LED_ANIM_PULSAR, LED_ANIM_VOICE, LED_ANIM_VORTEX};
    for (int m = 0; m < 3; ++m) {
        anim_tick = 59; // Phase 60 is between both pulsar waves.
        test_now += 31000;
        leds_update(false, true, modes[m], 0);
        if (check_transfer()) return 1;
        int black = 0;
        for (int i = LED_BASE_CHAIN_COUNT; i < LED_RING_COUNT; ++i) black += test_dma_words[i] == 0;
        if ((m < 2 && black != LED_EXPERIMENT_TAIL_FRAMES) ||
            (m == 2 && (black == 0 || black == LED_EXPERIMENT_TAIL_FRAMES))) {
            printf("FAIL: spare background must be truly black in mode %d\n", modes[m]);
            failures++;
        }
    }
    return failures;
}

static int check_polish(void) {
    int failures = 0;
    unsigned previous[LED_EXPERIMENT_TAIL_FRAMES] = {0};
    for (int frame = 0; frame < 190; ++frame) {
        if (check_frame()) return 1;
        for (int i = 0; i < LED_EXPERIMENT_TAIL_FRAMES; ++i) {
            uint32_t word = test_dma_words[LED_BASE_CHAIN_COUNT + i] >> 8;
            unsigned r = (word >> 8) & 255, g = (word >> 16) & 255, b = word & 255;
            unsigned peak = r > g ? r : g;
            if (b > peak) peak = b;
            if (peak > 7) failures |= 1;
#if CELESTIAL_RING_EXPERIMENT
            int64_t elapsed = test_now - (int64_t)LED_BOOT_COLOR_MS * 3000;
            unsigned phase = (unsigned)(elapsed / ((int64_t)LED_DIAG_COLOR_MS * 1000)) % 3;
            uint32_t expected = phase == 0 ? 0x00070000 : phase == 1 ? 0x07000000 : 0x00000700;
            if (test_dma_words[LED_BASE_CHAIN_COUNT + i] != expected) failures |= 1;
#else
            if ((frame && abs((int)peak - (int)previous[i]) > 1) || (r && g && b)) failures |= 1;
#endif
            previous[i] = peak;
        }
    }
    if (failures & 1) puts("FAIL: spare rainbow still has full-brightness surges");
    const uint8_t levels[] = {20, 60, 90};
    unsigned totals[3] = {0};
    for (int j = 0; j < 3; ++j) {
        test_now += 31000;
        leds_update(false, true, LED_ANIM_VOICE, levels[j]);
        if (check_transfer()) return 1;
        for (int i = LED_BASE_CHAIN_COUNT; i < LED_RING_COUNT; ++i) {
            uint32_t word = test_dma_words[i] >> 8;
            totals[j] += ((word >> 8) & 255) + ((word >> 16) & 255) + (word & 255);
        }
    }
    if (totals[0] > 24 || totals[1] < totals[0] + 20 || totals[2] < totals[1] + 20) {
        puts("FAIL: spare voice ring must remain quiet at low levels and fill before the largest ring");
        failures |= 2;
    }
    int64_t animation_time = test_now;
    if (encoder_display_stall_probe()) {
        puts("FAIL: encoder must emit one step per full cycle, none for half cycle/bounce");
        failures |= 4;
    }
    test_now = animation_time; // Encoder probe resets its independent mock clock.
    return failures != 0;
}

int main(void) {
    if (LED_RING_COUNT != 59 || LED_BRIGHTNESS_LIMIT != 4 || AUDIO_STARTUP_SOUNDS) {
        puts("FAIL: experiment needs 43+16 frames, 4% cap and silent startup");
        return 1;
    }
    test_now = 0;
    test_fifo_count = accepted_pixels = 0;
    leds_init();
    if (accepted_pixels != 59) return 1;
    if (test_dma_wait_count != 1 || check_transfer()) {
        puts("FAIL: whole LED frame must use PIO-paced DMA and a >=300us reset after drain");
        return 1;
    }
    leds_set_brightness(100);
    if (rgb_to_grb(255, 255, 255) != 0x0a0a0a) return 1;
    /* Boot colors must reach every physical address, independent of menu/effect. */
    const int64_t phase_times[] = {100000, 2100000, 4100000};
    const uint32_t phase_words[] = {0x000a0000, 0x0a000000, 0x00000a00};
    for (int phase = 0; phase < 3; ++phase) {
        test_now = phase_times[phase];
        uint waits = test_dma_wait_count;
        leds_update(true, true, LED_ANIM_VOICE, 0);
        if (test_dma_wait_count != waits + 1 || check_transfer()) return 1;
        for (int i = 0; i < 59; ++i) if (test_dma_words[i] != phase_words[phase]) {
            printf("FAIL: boot color %d missing at pixel %d\n", phase, i);
            return 1;
        }
    }
    test_now = 6100000;
    if (check_dark_backgrounds()) return 1;
    active_anim_mode = LED_ANIM_RAINBOW;
    if (check_frame()) return 1;
    bool spatial_color = false;
    uint32_t first_word = test_dma_words[LED_BASE_CHAIN_COUNT];
    for (int i = LED_BASE_CHAIN_COUNT + 1; i < LED_RING_COUNT; ++i) {
        spatial_color |= test_dma_words[i] != first_word;
    }
#if CELESTIAL_RING_EXPERIMENT
    if (spatial_color) {
        puts("FAIL: diagnostic spare must hold a uniform pure RGB frame");
        return 1;
    }
#else
    if (!spatial_color) {
        puts("FAIL: fourth ring still has uniform diagnostic color instead of a rainbow");
        return 1;
    }
#endif
    if (check_polish()) return 1;
    for (active_anim_mode = 0; active_anim_mode < LED_ANIM_COUNT; ++active_anim_mode) {
        if (check_frame()) {
            printf("FAIL: appended frames absent in animation %d\n", active_anim_mode);
            return 1;
        }
    }
    active_anim_mode = LED_ANIM_RAINBOW;
    uint32_t initial = test_dma_words[LED_BASE_CHAIN_COUNT];
    bool changed = false;
    for (int tick = 0; tick < 500; ++tick) {
        if (check_frame()) return 1;
        changed |= test_dma_words[LED_BASE_CHAIN_COUNT] != initial;
    }
    if (!changed) return 1;
    /* At the same animation tick, the microphone must change the actual wire frame. */
    uint32_t quiet[LED_EXPERIMENT_TAIL_FRAMES];
    uint32_t saved_tick = anim_tick;
    test_now += 31000;
    leds_update(false, true, LED_ANIM_VOICE, 0);
    for (int i = 0; i < LED_EXPERIMENT_TAIL_FRAMES; ++i) quiet[i] = test_dma_words[LED_BASE_CHAIN_COUNT + i];
    anim_tick = saved_tick;
    test_now += 31000;
    leds_update(false, true, LED_ANIM_VOICE, 100);
    bool voice_changed = false;
    for (int i = 0; i < LED_EXPERIMENT_TAIL_FRAMES; ++i) voice_changed |= quiet[i] != test_dma_words[LED_BASE_CHAIN_COUNT + i];
    if (!voice_changed || check_transfer()) {
        puts("FAIL: fourth-ring voice effect ignores microphone level");
        return 1;
    }
    /* Real audio state must override the menu; different notes change tail colors. */
    active_chiptune_track = 0;
    live_chiptune_freq = 220.0f;
    saved_tick = anim_tick;
    test_now += 31000;
    leds_update(false, true, LED_ANIM_VOICE, 0);
    for (int i = 0; i < LED_EXPERIMENT_TAIL_FRAMES; ++i) quiet[i] = test_dma_words[LED_BASE_CHAIN_COUNT + i];
    anim_tick = saved_tick;
    live_chiptune_freq = 880.0f;
    test_now += 31000;
    leds_update(false, true, LED_ANIM_VOICE, 0);
    bool note_changed = false;
    for (int i = 0; i < LED_EXPERIMENT_TAIL_FRAMES; ++i) note_changed |= quiet[i] != test_dma_words[LED_BASE_CHAIN_COUNT + i];
    if (!note_changed || check_transfer()) return 1;
    live_chiptune_freq = 0.0f; // Note rests still need a complete valid frame.
    if (check_frame()) return 1;
    active_chiptune_track = -1;
    test_now += 31000;
    leds_update(true, true, LED_ANIM_VOICE, 0);
    if (check_transfer()) return 1;
    if (test_dma_words[LED_BASE_CHAIN_COUNT] == test_dma_words[LED_BASE_CHAIN_COUNT + 4]) return 1;
    puts("PASS: 59-word DMA, dark effect backgrounds, bounded/nonwhite rainbow or pure RGB diagnostic, mic/note response and encoder");
    return 0;
}
