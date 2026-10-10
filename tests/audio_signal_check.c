/* Native C check of the production PCM generator. No Python runner. */
#include "firmware_harness.c"
#undef printf

static int32_t left_sample(const uint32_t *words, size_t frame) {
#if defined(AUDIO_I2S_SLOT_BITS) && AUDIO_I2S_SLOT_BITS == 32
    return (int32_t)words[frame * 2];
#else
    return (int16_t)(words[frame] >> 16);
#endif
}

static int32_t right_sample(const uint32_t *words, size_t frame) {
#if defined(AUDIO_I2S_SLOT_BITS) && AUDIO_I2S_SLOT_BITS == 32
    return (int32_t)words[frame * 2 + 1];
#else
    return (int16_t)words[frame];
#endif
}

static int check_voice_resolution(void) {
    int8_t ramp[241];
    for (int i = 0; i < 241; ++i) ramp[i] = (int8_t)(i - 120);
    voice_clip_t clip = { "PCM RAMP", ramp, sizeof ramp };
    uint32_t words[1024] = {0};
    audio_set_volume(10);
    current_freq = 0; total_note_samples = samples_played = 0;
    active_voice_clip = &clip; voice_sample_idx = 0; voice_active = true;
    fill_audio_buffer(words, 512);
    int distinct = 1, mismatch = 0;
    for (size_t i = 1; i < sizeof ramp; ++i) {
        if (left_sample(words, i) != left_sample(words, i - 1)) ++distinct;
        if (left_sample(words, i) != right_sample(words, i)) ++mismatch;
    }
    printf("V10 voice ramp: %d/241 distinct levels; channel mismatches=%d\n", distinct, mismatch);
#if defined(AUDIO_I2S_SLOT_BITS) && AUDIO_I2S_SLOT_BITS == 32
    return distinct >= 230 && mismatch == 0 ? 0 : 1;
#else
    return distinct >= 40 && mismatch == 0 ? 0 : 1;
#endif
}

static int check_mute_and_tone(void) {
    uint32_t words[1024] = {0};
    voice_active = false; active_voice_clip = NULL;
    audio_set_volume(0); audio_play_tone(440, 1000);
    fill_audio_buffer(words, 512);
    for (size_t i = 0; i < 512; ++i) if (words[i]) return 1;
    audio_set_volume(10); audio_play_tone(440, 1000);
    fill_audio_buffer(words, 512);
    int distinct = 0; int32_t values[256];
    for (size_t i = 0; i < 256; ++i) {
        int32_t sample = left_sample(words, i);
        if (sample != right_sample(words, i)) return 1;
        bool seen = false;
        for (int j = 0; j < distinct; ++j) if (values[j] == sample) seen = true;
        if (!seen) values[distinct++] = sample;
    }
    printf("V10 tone: %d distinct levels in 256 frames; V0 silence exact\n", distinct);
#if defined(AUDIO_I2S_SLOT_BITS) && AUDIO_I2S_SLOT_BITS == 32
    return distinct >= 100 ? 0 : 1;
#else
    return distinct >= 35 ? 0 : 1;
#endif
}

static double sine_rms(uint8_t volume) {
    uint32_t words[1024];
    audio_set_volume(volume); audio_play_diagnostic_tone();
#if defined(AUDIO_I2S_SLOT_BITS) && AUDIO_I2S_SLOT_BITS == 32
    fill_audio_buffer(words, 1024);
#else
    fill_audio_buffer(words, 512);
#endif
    if (samples_played != 512 || current_freq != 440.0f) return 0;
    double squares = 0;
    for (size_t i = 192; i < 512; ++i) {
        double value = (double)left_sample(words, i);
        squares += value * value;
    }
    return sqrt(squares / 320.0);
}

static int check_diagnostic_controls(void) {
    double quiet = sine_rms(5), louder = sine_rms(10);
    double ratio = quiet > 0 ? louder / quiet : 0;
    audio_play_chime(); audio_play_voice(VOICE_ONLINE); audio_stop_all();
    bool stopped = !audio_is_playing();
    printf("Diagnostic sine: V10/V5 RMS=%.3f (expected4); 512 frames; stop-all=%s\n",
           ratio, stopped ? "OK" : "FAIL");
    return fabs(ratio - 4.0) < 0.06 && stopped ? 0 : 1;
}

static int check_late_dma_irq(void) {
    next_dma = 0; audio_init();
    test_force_dma_idle = true;
    test_dma_irq_flags = (1u << dma_chan_a) | (1u << dma_chan_b);
    uint32_t starts_before = test_dma_start_count;
    audio_dma_irq_handler();
    bool restarted = test_dma_start_count == starts_before + 1 && !test_dma_irq_flags;
    test_force_dma_idle = false;
    printf("Explicitly idle DMA pair: defensive restart=%s\n", restarted ? "OK" : "FAIL");
    return restarted ? 0 : 1;
}

static int check_dma_read_bounds(void) {
    next_dma = 0; audio_init();
    bool bounded = test_dma_configs[dma_chan_a].ring_bits == 12 &&
        test_dma_configs[dma_chan_b].ring_bits == 12 &&
        !test_dma_configs[dma_chan_a].ring_write && !test_dma_configs[dma_chan_b].ring_write &&
        ((uintptr_t)buf_a & 4095u) == 0 && ((uintptr_t)buf_b & 4095u) == 0 &&
        sizeof buf_a == 4096 && sizeof buf_b == 4096;
    audio_play_diagnostic_tone();
    test_dma_busy_mask = 1u << dma_chan_a; // A has already been retriggered by B.
    test_dma_irq_flags = 1u << dma_chan_a;
    uint32_t before = samples_played;
    audio_dma_irq_handler();
    bool active_not_refilled = samples_played == before;
    test_dma_busy_mask = 1u << dma_chan_b; // B is close to handing off to A.
    test_dma_hw[dma_chan_b].transfer_count = 511;
    test_dma_irq_flags = 1u << dma_chan_a;
    audio_dma_irq_handler();
    bool near_handoff_not_refilled = samples_played == before;
    test_dma_busy_mask = UINT32_MAX;
    audio_stop_all();
    printf("DMA read-ring bounds / skip active or near-handoff refill: %s\n",
           bounded && active_not_refilled && near_handoff_not_refilled ? "OK" : "FAIL");
    return bounded && active_not_refilled && near_handoff_not_refilled ? 0 : 1;
}

int main(void) {
    int failures = check_voice_resolution();
    failures += check_mute_and_tone();
    failures += check_diagnostic_controls();
    failures += check_late_dma_irq();
    failures += check_dma_read_bounds();
    failures += main_boot_sequence_probe();
    failures += main_boot_announcement_probe();
    printf("Configured boot mode: %s\n", AUDIO_DIAGNOSTIC_BOOT ? "muted; no automatic playback" : "chime then voice");
    /* Reuse the existing C probes directly; no Python orchestration required. */
    int (*existing_probes[])(void) = {
        query_bounce_probe, query_boot_hold_probe, encoder_display_stall_probe,
        led_frames_probe, hsv_probe, led_brightness_probe,
        oled_timeout_probe, oled_init_failure_probe, oled_scan_probe,
        audio_short_wake_probe, voice_invalid_probe,
        mic_peak_probe, mic_ring_order_probe, mic_live_cursor_probe,
        main_cyw43_failure_probe, main_listening_switch_probe, main_query_release_probe
    };
    for (size_t i = 0; i < sizeof existing_probes / sizeof existing_probes[0]; ++i) {
        int result = existing_probes[i]();
        if (result) { printf("Existing C probe %zu failed: %d\n", i + 1, result); ++failures; }
    }
    printf("Existing C control/driver probes executed: 17\n");
    printf("Audio PCM check: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
