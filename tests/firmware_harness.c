#include "pico_stubs.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
static char test_logs[32768];
static int test_printf(const char *fmt, ...) {
    va_list args; va_start(args,fmt);
    size_t used=strlen(test_logs);
    int n=vsnprintf(test_logs+used,sizeof(test_logs)-used,fmt,args);
    va_end(args); return n;
}
#define printf test_printf
#include "../firmware/src/query_button.c"
#include "../firmware/src/encoder.c"
#include "../firmware/src/sh1106.c"
#include "../firmware/src/audio.c"
#include "../firmware/src/mic.c"
#include "../firmware/src/leds.c"
#include "../firmware/src/voice_data.c"
#define main firmware_main
#include "../firmware/src/main.c"
#undef main
#define EXPORT __declspec(dllexport)
static void reset_gpio(void) { memset(test_gpio, 1, sizeof test_gpio); test_now = 0; }

EXPORT int query_bounce_probe(void) {
    reset_gpio(); query_button_init();
    test_now=100000; test_gpio[PIN_QUERY_BTN]=false; query_button_update();
    test_now=131000; query_button_update();
    if (!query_button_just_pressed()) return 1;
    test_now=140000; test_gpio[PIN_QUERY_BTN]=true; query_button_update();
    test_now=142000; test_gpio[PIN_QUERY_BTN]=false; query_button_update();
    if (!query_button_is_held() || query_button_just_released()) return 2;
    test_now=200000; test_gpio[PIN_QUERY_BTN]=true; query_button_update();
    test_now=231000; query_button_update();
    if (!query_button_just_released() || query_button_is_held()) return 3;
    return 0;
}
EXPORT int query_boot_hold_probe(void) {
    reset_gpio(); test_gpio[PIN_QUERY_BTN]=false; query_button_init();
    query_button_update(); test_now=31000; query_button_update();
    return query_button_just_pressed() && query_button_is_held() ? 0 : 1;
}
EXPORT int encoder_display_stall_probe(void) {
    reset_gpio(); encoder_init();
    // A complete cycle can happen while main is inside a 24 ms OLED write.
    test_gpio_transition(PIN_ENCODER_A,false);
    test_gpio_transition(PIN_ENCODER_B,false);
    encoder_update();
    if (encoder_get_delta() != 0) return 1; // A half cycle must not skip a menu item.
    test_gpio_transition(PIN_ENCODER_A,true);
    test_gpio_transition(PIN_ENCODER_B,true);
    encoder_update();
    if (encoder_get_delta() != 1 || encoder_get_position() != 1) return 2;
    // Contact bounce returns to the same state without a full cycle.
    test_gpio_transition(PIN_ENCODER_A,false);
    test_gpio_transition(PIN_ENCODER_A,true);
    encoder_update();
    if (encoder_get_delta() != 0) return 3;
    test_gpio_transition(PIN_ENCODER_B,false);
    test_gpio_transition(PIN_ENCODER_A,false);
    test_gpio_transition(PIN_ENCODER_B,true);
    test_gpio_transition(PIN_ENCODER_A,true);
    encoder_update();
    return encoder_get_delta() == -1 && encoder_get_position() == 0 ? 0 : 4;
}
EXPORT int led_frames_probe(void) {
    test_now=0; test_fifo_count=accepted_pixels=0; leds_init();
    leds_set_all_rgb(255,0,0); leds_set_all_rgb(0,255,0);
    return accepted_pixels == (3 * LED_RING_COUNT) ? 0 : (int)accepted_pixels;
}
EXPORT int hsv_probe(void) {
    uint8_t r,g,b; hsv_to_rgb(50,255,255,&r,&g,&b);
    return r == 255 && g == 212 && b == 0 ? 0 : g;
}
EXPORT int led_brightness_probe(void) {
    leds_set_brightness(100);
    uint32_t pixel=rgb_to_grb(255,255,255);
    return pixel == 0x2d2d2d ? 0 : 1;
}
EXPORT int oled_timeout_probe(void) {
    blocking_i2c_calls=0; i2c_fail_command=i2c_fail_payload=0;
    sh1106_init(i2c1,0x3c);
    return blocking_i2c_calls;
}
EXPORT int oled_init_failure_probe(void) {
    i2c_fail_command=0; i2c_fail_payload=1;
    bool ok=sh1106_init(i2c1,0x3c); i2c_fail_payload=0;
    return ok ? 1 : 0;
}
EXPORT int oled_scan_probe(void) { return sh1106_i2c_scan(i2c1) == 0x3c ? 0 : 1; }
EXPORT int audio_short_wake_probe(void) {
    audio_set_volume(10);
    test_now=2000000; amp_is_sleeping=true; silence_since=0;
    chime_index=-1; active_chiptune_track=-1; voice_active=false;
    audio_play_blip(); uint32_t scratch[512]; fill_audio_buffer(scratch,512);
    test_now += 2000; audio_update();
    return amp_is_sleeping ? 1 : 0;
}
EXPORT int voice_invalid_probe(void) {
    return voice_get_clip((voice_phrase_id_t)-1) == NULL ? 0 : 1;
}
EXPORT int mic_peak_probe(void) {
    test_now=100000; last_metric_update=0; mic_dma_chan=0;
    dc_filter_x=dc_filter_y=0;
    memset((void *)mic_ring_buf,0,sizeof mic_ring_buf);
    mic_ring_buf[0]=0x40000000; // Sign bit at bit 30 -> -32768 after conversion.
    test_dma_hw[0].write_addr=(uintptr_t)mic_ring_buf;
    mic_update();
    return mic_get_peak_amplitude() == 32767 && mic_get_level_percent() == 100 ? 0 : mic_get_peak_amplitude();
}
EXPORT int mic_ring_order_probe(void) {
    mic_dma_chan=0; test_now=100000; last_metric_update=0;
    dc_filter_x=dc_filter_y=0;
    memset((void *)mic_ring_buf,0,sizeof mic_ring_buf);
    mic_ring_buf[0]=0x13880000; // +10000, packed with sign at bit 30.
    mic_ring_buf[1]=0x6c780000; // -10000.
    test_dma_hw[0].write_addr=(uintptr_t)&mic_ring_buf[0];
    mic_update(); int reference_peak=mic_get_peak_amplitude();
    test_now=200000; last_metric_update=0; dc_filter_x=dc_filter_y=0;
    memset((void *)mic_ring_buf,0,sizeof mic_ring_buf);
    mic_ring_buf[255]=0x13880000; mic_ring_buf[0]=0x6c780000;
    test_dma_hw[0].write_addr=(uintptr_t)&mic_ring_buf[255];
    mic_update();
    return mic_get_peak_amplitude() == reference_peak ? 0 : 1;
}
static int cursor_reads;
static void mic_cursor_advance(void) {
    if (++cursor_reads == 2) {
        mic_ring_buf[0]=0x27100000; // DMA replaced the oldest sample with +20000.
        test_dma_hw[0].write_addr=(uintptr_t)&mic_ring_buf[1];
    }
}
EXPORT int mic_live_cursor_probe(void) {
    mic_dma_chan=0; test_now=100000; last_metric_update=0;
    dc_filter_x=dc_filter_y=0;
    memset((void *)mic_ring_buf,0,sizeof mic_ring_buf);
    mic_ring_buf[0]=0x13880000; mic_ring_buf[1]=0x6c780000;
    test_dma_hw[0].write_addr=(uintptr_t)&mic_ring_buf[0];
    cursor_reads=0; test_dma_cursor_hook=mic_cursor_advance;
    mic_update(); test_dma_cursor_hook=NULL;
    return mic_get_peak_amplitude() >= 20000 ? 0 : 1;
}
static void test_main_tick(void) {
    main_loop_count++;
    if (main_scenario == 6 && main_loop_count % 16 == 0) {
        uint32_t scratch[AUDIO_BUFFER_WORDS];
        fill_audio_buffer(scratch,AUDIO_BUFFER_WORDS); // Simulate 32 ms sample generation only.
    }
    if (main_scenario == 1 && main_loop_count == 1) {
        current_mode_idx=MODE_CHIPTUNE;
        current_state=STATE_LISTENING; sw_clicked_event=false;
        test_gpio[PIN_ENCODER_SW]=true;
        test_now += 101000;
    } else if (main_scenario == 1 && main_loop_count == 2) {
        sw_clicked_event=true;
    } else if (main_scenario == 3 && main_loop_count == 1) {
        current_state=STATE_LISTENING; released_duration=100;
    }
    if (main_loop_count >= 650 || ((main_scenario == 1 || main_scenario == 3 || main_scenario == 4) && main_loop_count >= 3)) longjmp(main_exit,1);
}
EXPORT int main_cyw43_failure_probe(void) {
    reset_gpio(); cyw43_fail=1; cyw43_uninitialized_calls=0;
    cyw43_ready=false; current_state=STATE_IDLE;
    next_dma=0; main_scenario=2; main_loop_count=0;
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0; cyw43_fail=0;
    return cyw43_uninitialized_calls;
}
EXPORT int main_listening_switch_probe(void) {
    reset_gpio(); cyw43_fail=0; next_dma=0; main_scenario=1; main_loop_count=0;
    volume_edit_mode=mic_test_mode=led_edit_mode=chiptune_mode=voice_mode=false;
    easter_egg_active=false; active_track_idx=0;
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0;
    return current_state == STATE_LISTENING && !chiptune_mode ? 0 : 1;
}
EXPORT int main_query_release_probe(void) {
    reset_gpio(); cyw43_fail=0; cyw43_ready=false; next_dma=0;
    main_scenario=3; main_loop_count=0; current_state=STATE_IDLE;
    volume_edit_mode=mic_test_mode=led_edit_mode=chiptune_mode=voice_mode=false;
    test_logs[0]=0;
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0;
    return current_state == STATE_IDLE && strstr(test_logs,"Upload complete") == NULL ? 0 : 1;
}
EXPORT int main_boot_sequence_probe(void) {
    reset_gpio(); cyw43_fail=0; cyw43_ready=false; next_dma=0;
    main_scenario=4; main_loop_count=0; current_state=STATE_IDLE;
    voice_active=false; active_voice_clip=NULL;
    chime_index=-1; active_chiptune_track=-1; audio_set_volume(AUDIO_DEFAULT_VOLUME);
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0;
    return ((AUDIO_DIAGNOSTIC_BOOT || !AUDIO_STARTUP_SOUNDS) ?
            (chime_index == -1 && !voice_active && audio_get_volume() == 0) :
            (chime_index == 0 && !voice_active)) ? 0 : 1;
}
EXPORT int main_boot_announcement_probe(void) {
    reset_gpio(); cyw43_fail=0; cyw43_ready=false; next_dma=0;
    main_scenario=6; main_loop_count=0; current_state=STATE_IDLE;
    voice_active=false; active_voice_clip=NULL; active_chiptune_track=-1;
    chime_index=-1; audio_set_volume(AUDIO_DEFAULT_VOLUME);
    volume_edit_mode=mic_test_mode=led_edit_mode=chiptune_mode=voice_mode=false;
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0;
    if (AUDIO_DIAGNOSTIC_BOOT || !AUDIO_STARTUP_SOUNDS) return chime_index == -1 && !voice_active && audio_get_volume() == 0 ? 0 : 1;
    return chime_index == -1 && voice_active && active_voice_clip == voice_get_clip(VOICE_ONLINE) ? 0 : 1;
}
