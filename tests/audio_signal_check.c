/* Native C check of the production PCM generator. No Python runner. */
#include "firmware_harness.c"
#define main isolated_probe_main
#include "../firmware/src/speaker_probe.c"
#undef main
#undef printf

static int check_isolated_probe(void) {
    probe_play(true, 8);
    if (probe_min != 0 || probe_max != 16) return 1;
    for (size_t i = 0; i < PROBE_FRAMES + PROBE_TAIL; ++i) {
        int16_t left = (int16_t)(probe_words[i] >> 16);
        int16_t right = (int16_t)(probe_words[i] & 0xffff);
        if (left != right || left < 0 || left > 16 || (i >= PROBE_FRAMES && left != 0)) return 1;
    }
    probe_play(false, 8);
    if (probe_min != -8 || probe_max != 8) return 1;
    for (size_t i = 0; i < PROBE_FRAMES + PROBE_TAIL; ++i) {
        int16_t left = (int16_t)(probe_words[i] >> 16);
        int16_t right = (int16_t)(probe_words[i] & 0xffff);
        if (left != right || left < -8 || left > 8 || (i >= PROBE_FRAMES && left != 0)) return 1;
    }
    probe_play(true, 16);
    if (probe_min != 0 || probe_max != 32) return 1;
    probe_stop();
    if (probe_active || test_gpio[PIN_SPK_BCLK] || test_gpio[PIN_SPK_LRC] || test_gpio[PIN_SPK_DIN]) return 1;
    puts("Isolated probe: positive/bipolar signed packing, bounded levels, zero tail, stop pins PASS");
    return 0;
}

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

static int check_volume_faults(void) {
    double low = sine_rms(1);
    next_dma = 0; audio_init();
    float boot_gain = master_gain;
    audio_set_volume(AUDIO_DEFAULT_VOLUME);
    bool same_gain = fabsf(master_gain - boot_gain) < 0.0000001f;
    audio_set_volume(AUDIO_DEFAULT_VOLUME + 1);
    bool rising_gain = master_gain > boot_gain;
    for (size_t i = 0; i < AUDIO_BUFFER_WORDS; ++i) buf_a[i] = buf_b[i] = 123;
    audio_set_volume(0);
    bool muted = true;
    for (size_t i = 0; i < AUDIO_BUFFER_WORDS; ++i) if (buf_a[i] || buf_b[i]) muted = false;
    printf("V1 nonzero PCM: %s; V0 clears queued PCM: %s\n", low > 0 ? "OK" : "FAIL", muted ? "OK" : "FAIL");
    printf("Boot/encoder gain equality: %s; first clockwise step louder: %s\n", same_gain ? "OK" : "FAIL", rising_gain ? "OK" : "FAIL");
    return low > 0 && muted && same_gain && rising_gain ? 0 : 1;
}

static int check_diagnostic_controls(void) {
    double quiet = sine_rms(5), louder = sine_rms(10);
    double ratio = quiet > 0 ? louder / quiet : 0;
    audio_play_chime(); audio_play_voice(VOICE_ONLINE); audio_stop_all();
    bool stopped = !audio_is_playing();
    double expected = pow(10.0, (AUDIO_VOLUME_RANGE_DB * 5.0 / 99.0) / 20.0);
    printf("Diagnostic sine: V10/V5 RMS=%.3f (expected%.3f); 512 frames; stop-all=%s\n",
           ratio, expected, stopped ? "OK" : "FAIL");
    return fabs(ratio - expected) < 0.01 && stopped ? 0 : 1;
}

static int check_late_dma_irq(void) {
    next_dma = 0; audio_init();
    audio_set_volume(10); audio_play_tone(440, 1000);
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
    bool bounded = test_dma_configs[dma_chan_a].ring_bits == AUDIO_DMA_RING_BITS &&
        test_dma_configs[dma_chan_b].ring_bits == AUDIO_DMA_RING_BITS &&
        !test_dma_configs[dma_chan_a].ring_write && !test_dma_configs[dma_chan_b].ring_write &&
        ((uintptr_t)buf_a & ((1u << AUDIO_DMA_RING_BITS) - 1)) == 0 && ((uintptr_t)buf_b & ((1u << AUDIO_DMA_RING_BITS) - 1)) == 0 &&
        sizeof buf_a == (1u << AUDIO_DMA_RING_BITS) && sizeof buf_b == (1u << AUDIO_DMA_RING_BITS);
    audio_set_volume(10);
    audio_play_diagnostic_tone();
    test_dma_busy_mask = 1u << dma_chan_a; // A has already been retriggered by B.
    test_dma_irq_flags = 1u << dma_chan_a;
    uint32_t before = samples_played;
    audio_dma_irq_handler();
    bool active_not_refilled = samples_played == before;
    test_dma_busy_mask = 1u << dma_chan_b; // B is close to handing off to A.
    test_dma_hw[dma_chan_b].transfer_count = AUDIO_BUFFER_WORDS / 2 - 1;
    test_dma_irq_flags = 1u << dma_chan_a;
    audio_dma_irq_handler();
    bool near_handoff_not_refilled = samples_played == before;
    test_dma_busy_mask = UINT32_MAX;
    audio_stop_all();
    printf("DMA read-ring bounds / skip active or near-handoff refill: %s\n",
           bounded && active_not_refilled && near_handoff_not_refilled ? "OK" : "FAIL");
    return bounded && active_not_refilled && near_handoff_not_refilled ? 0 : 1;
}

static int check_stream_bounds(void) {
    audio_stream_reset();
    static int16_t input[AUDIO_STREAM_RING_SIZE + 1];
    for (size_t i = 0; i < sizeof input / sizeof input[0]; ++i) input[i] = (int16_t)i;
    audio_stream_push(input, AUDIO_STREAM_RING_SIZE);
    uint32_t before = audio_stream_write_idx;
    int16_t oldest = audio_stream_ring[0];
    audio_stream_push(input + AUDIO_STREAM_RING_SIZE, 1);
    if (audio_stream_write_idx != before || audio_stream_ring[0] != oldest) {
        puts("FAIL: full response ring overwrites unplayed samples");
        return 1;
    }
    audio_stream_reset();
    puts("Response ring: backpressure preserves unplayed samples PASS");
    audio_set_volume(0);
    audio_stream_push(input, 512);
    audio_stream_finish();
    if (audio_is_stream_playing()) {
        puts("FAIL: muted response waits forever for stopped DMA");
        audio_set_volume(10);
        audio_stream_reset();
        return 1;
    }
    audio_set_volume(10);
    audio_stream_reset();
    return 0;
}

static int check_stream_dynamics(void) {
    audio_stop_all();
    audio_set_volume(100);
    int16_t ramp[512];
    for (size_t i = 0; i < 512; ++i) ramp[i] = (int16_t)(1000 + i * 20);
    audio_stream_push(ramp, 512);
    audio_stream_finish();
    uint32_t words[512];
    fill_audio_buffer(words, 512);
    for (size_t i = 1; i < 512; ++i) {
        if (left_sample(words, i) <= left_sample(words, i - 1)) {
            puts("FAIL: full-scale server PCM gets microphone preamp and clips");
            audio_stop_all();
            return 1;
        }
    }
    audio_stop_all();
    puts("Server PCM: dynamic range preserved without microphone preamp PASS");
    return 0;
}

int main(void) {
    next_dma = 0; audio_init();
    int failures = check_isolated_probe();
    failures += check_voice_resolution();
    failures += check_mute_and_tone();
    failures += check_diagnostic_controls();
    failures += check_volume_faults();
    failures += check_late_dma_irq();
    failures += check_dma_read_bounds();
    failures += check_stream_bounds();
    failures += check_stream_dynamics();
    sh1106_clear();
    sh1106_draw_response_marquee("HELLO FULL SCREEN", 0);
    uint8_t first_frame[SH1106_BUF_SIZE];
    memcpy(first_frame, display_buffer, sizeof first_frame);
    size_t lit = 0;
    for (size_t i = 0; i < sizeof display_buffer; ++i)
        for (unsigned bit = 0; bit < 8; ++bit) lit += (display_buffer[i] >> bit) & 1;
    sh1106_clear();
    sh1106_draw_response_marquee("HELLO FULL SCREEN", 600);
    if (lit < 1800 || !memcmp(first_frame, display_buffer, sizeof first_frame)) {
        puts("FAIL: response must occupy full-height large glyphs and scroll");
        ++failures;
    } else puts("OLED response: full-height glyphs and moving frame PASS");
    failures += main_boot_sequence_probe();
    failures += main_boot_announcement_probe();
    printf("Configured boot mode: %s\n", !AUDIO_STARTUP_SOUNDS ? "muted; no automatic playback" : "chime then voice");
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
