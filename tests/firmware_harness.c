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
#include "../firmware/src/audio_ring_buffer.c"
#include "../firmware/src/audio.c"
#include "../firmware/src/mic.c"
#include "../firmware/src/leds.c"
#include "../firmware/src/voice_data.c"
#include "../firmware/src/music_data.c"
#include "../firmware/src/net_dispatcher.c"
#include "../firmware/src/net_client.h"
void net_client_init(void) {}
void net_client_update(void) {}
net_status_t net_client_get_status(void) { return NET_STATUS_DISCONNECTED; }
bool net_client_is_connected(void) { return false; }
bool net_client_is_server_ready(void) { return false; }
bool net_client_ping_server(void) { return true; }
const char* net_client_get_ip_str(void) { return "0.0.0.0"; }
const char* net_client_get_active_ssid(void) { return "TEST_SSID"; }
const char* net_client_get_server_ip_str(void) { return "127.0.0.1"; }
uint16_t net_client_get_server_port(void) { return 5005; }
net_host_type_t net_client_get_host_type(void) { return HOST_TYPE_PI5; }
const char* net_client_get_host_name(void) { return "Pi 5 Brain"; }
void net_client_set_preferred_host(net_host_type_t pref) { (void)pref; }
net_host_type_t net_client_get_preferred_host(void) { return HOST_TYPE_PI5; }
uint8_t net_client_get_model_count(void) { return 4; }
const char* net_client_get_model_name(uint8_t idx) { (void)idx; return "DeepSeek V3"; }
const char* net_client_get_model_tag(uint8_t idx) { (void)idx; return "DSK"; }
uint8_t net_client_get_model_theme(uint8_t idx) { (void)idx; return 0; }
uint8_t net_client_get_model_id(uint8_t idx) { return idx; }
bool net_client_request_catalog(void) { return true; }
bool net_client_poll_remote_command(remote_cmd_t *cmd) { if (cmd) cmd->has_command = false; return false; }
void net_client_send_telemetry(uint8_t mode, uint8_t state, uint8_t volume, uint8_t anim, uint8_t clock_style,
                               uint8_t lang, uint8_t host_type, uint8_t pref_host,
                               uint8_t ai_model, uint8_t mic_level, uint8_t cosmos_zoom, uint8_t is_audio_playing) {
    (void)mode; (void)state; (void)volume; (void)anim; (void)clock_style; (void)lang; (void)host_type; (void)pref_host;
    (void)ai_model; (void)mic_level; (void)cosmos_zoom; (void)is_audio_playing;
}
bool net_client_has_paired_companion(void) { return false; }
bool net_client_has_unsolicited_text(void) { return false; }
void net_client_clear_unsolicited_text(void) {}
bool net_client_send_query_start(uint8_t l, uint8_t m) { (void)l; (void)m; return true; }
bool net_client_send_audio_chunk(const int16_t *s, size_t c) { (void)s; (void)c; return true; }
bool net_client_send_query_end(void) { return true; }
void net_client_cancel(void) {}
bool net_client_check_and_clear_cancel(void) { return false; }
bool net_client_is_receiving_response(void) { return false; }
bool net_client_has_response_ready(void) { return false; }
void net_client_consume_response(void) {}
const char* net_client_get_response_text(void) { return ""; }
bool net_client_response_failed(void) { return false; }
uint32_t net_client_response_progress_ms(void) { return to_ms_since_boot(get_absolute_time()); }

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
    volume_edit_mode=mic_test_mode=led_edit_mode=chiptune_mode=voice_mode=clock_style_edit_mode=clock_time_edit_mode=false;
    easter_egg_active=false; active_track_idx=0;
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0;
    return current_state == STATE_LISTENING && !chiptune_mode ? 0 : 1;
}
EXPORT int main_query_release_probe(void) {
    reset_gpio(); cyw43_fail=0; cyw43_ready=false; next_dma=0;
    main_scenario=3; main_loop_count=0; current_state=STATE_IDLE;
    volume_edit_mode=mic_test_mode=led_edit_mode=chiptune_mode=voice_mode=clock_style_edit_mode=clock_time_edit_mode=false;
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
    volume_edit_mode=mic_test_mode=led_edit_mode=chiptune_mode=voice_mode=clock_style_edit_mode=clock_time_edit_mode=false;
    if (!setjmp(main_exit)) firmware_main();
    main_scenario=0;
    if (AUDIO_DIAGNOSTIC_BOOT || !AUDIO_STARTUP_SOUNDS) return chime_index == -1 && !voice_active && audio_get_volume() == 0 ? 0 : 1;
    return chime_index == -1 && voice_active && active_voice_clip == voice_get_clip(VOICE_ONLINE) ? 0 : 1;
}
static int dispatch_probe_hits = 0;
static uint8_t dispatch_probe_last_type = 0;
static void test_dispatch_handler(const net_packet_header_t *hdr, const uint8_t *payload, void *user_ctx) {
    (void)payload;
    if (user_ctx != NULL) *(int *)user_ctx += 1;
    dispatch_probe_hits++;
    dispatch_probe_last_type = hdr->msg_type;
}
EXPORT int net_dispatcher_probe(void) {
    net_packet_header_t out_hdr;
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    // 1. Short header rejection
    if (net_dispatch_validate_packet(buf, 10, &out_hdr) != NET_DISPATCH_ERR_SHORT_HEADER) return 1;

    // 2. Bad magic rejection
    net_packet_header_t hdr = { 0x12345678u, NET_PROTOCOL_VERSION, NET_MSG_HEARTBEAT_ACK, 1, 0, 0 };
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr), &out_hdr) != NET_DISPATCH_ERR_BAD_MAGIC) return 2;

    // 3. Bad version rejection
    hdr.magic = NET_PROTOCOL_MAGIC;
    hdr.version = 99;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr), &out_hdr) != NET_DISPATCH_ERR_BAD_VERSION) return 3;

    // 4. Valid heartbeat packet
    hdr.version = NET_PROTOCOL_VERSION;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr), &out_hdr) != NET_DISPATCH_OK) return 4;

    // 5. Payload bounds check for RESP_AUDIO_START (requires exactly 4 bytes)
    hdr.msg_type = NET_MSG_RESP_AUDIO_START;
    hdr.payload_len = 2;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 2, &out_hdr) != NET_DISPATCH_ERR_MALFORMED_PAYLOAD) return 5;
    hdr.payload_len = 4;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 4, &out_hdr) != NET_DISPATCH_OK) return 6;

    // 6. Odd byte count rejected for RESP_AUDIO_CHUNK
    hdr.msg_type = NET_MSG_RESP_AUDIO_CHUNK;
    hdr.payload_len = 3;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 3, &out_hdr) != NET_DISPATCH_ERR_MALFORMED_PAYLOAD) return 7;
    hdr.payload_len = 4;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 4, &out_hdr) != NET_DISPATCH_OK) return 8;

    // 7. APP_COMMAND requires >= 6 bytes; CATALOG_RESP requires >= 17 bytes
    hdr.msg_type = NET_MSG_APP_COMMAND;
    hdr.payload_len = 4;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 4, &out_hdr) != NET_DISPATCH_ERR_MALFORMED_PAYLOAD) return 9;
    hdr.payload_len = 6;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 6, &out_hdr) != NET_DISPATCH_OK) return 10;

    hdr.msg_type = NET_MSG_CATALOG_RESP;
    hdr.payload_len = 16;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 16, &out_hdr) != NET_DISPATCH_ERR_MALFORMED_PAYLOAD) return 11;
    hdr.payload_len = 17;
    memcpy(buf, &hdr, sizeof(hdr));
    if (net_dispatch_validate_packet(buf, sizeof(hdr) + 17, &out_hdr) != NET_DISPATCH_OK) return 12;

    // 8. Handler table lookup & process_packet execution
    static const net_dispatch_entry_t test_table[] = {
        { NET_MSG_APP_COMMAND,   test_dispatch_handler },
        { NET_MSG_CATALOG_RESP,  test_dispatch_handler }
    };
    if (net_dispatch_LookupHandler(test_table, 2, NET_MSG_APP_COMMAND) != test_dispatch_handler) return 13;
    if (net_dispatch_LookupHandler(test_table, 2, 0xFF) != NULL) return 14;

    int ctx_counter = 0;
    dispatch_probe_hits = 0;
    if (net_dispatch_process_packet(test_table, 2, buf, sizeof(hdr) + 17, &ctx_counter) != NET_DISPATCH_OK) return 15;
    if (dispatch_probe_hits != 1 || ctx_counter != 1 || dispatch_probe_last_type != NET_MSG_CATALOG_RESP) return 16;

    return 0;
}
EXPORT int audio_ring_buffer_probe(void) {
    int16_t backing[8];
    audio_ring_buffer_t rb;

    // 1. Reject non-power-of-two capacity
    if (audio_rb_init(&rb, backing, 6)) return 1;
    if (!audio_rb_init(&rb, backing, 8)) return 2;
    if (audio_rb_available_read(&rb) != 0 || audio_rb_available_write(&rb) != 8) return 3;

    // 2. Underflow on empty pop increments counter and outputs 0
    int16_t sample = 1234;
    if (audio_rb_pop_sample(&rb, &sample)) return 4;
    if (sample != 0 || audio_rb_get_underflow_count(&rb) != 1) return 5;

    // 3. Push 6 samples and verify FIFO order across wrap-around
    const int16_t in1[6] = { 10, 20, 30, 40, 50, 60 };
    if (!audio_rb_push_samples(&rb, in1, 6)) return 6;
    if (audio_rb_available_read(&rb) != 6 || audio_rb_available_write(&rb) != 2) return 7;

    // 4. Overflow rejected without corrupting queued samples
    const int16_t in_overflow[4] = { 70, 80, 90, 100 };
    if (audio_rb_push_samples(&rb, in_overflow, 4)) return 8;
    if (audio_rb_get_overflow_count(&rb) != 1 || audio_rb_available_read(&rb) != 6) return 9;

    // 5. Pop 4 samples, then push 4 more to wrap around power-of-two boundary
    for (int i = 0; i < 4; i++) {
        if (!audio_rb_pop_sample(&rb, &sample) || sample != (i + 1) * 10) return 10 + i;
    }
    if (!audio_rb_push_samples(&rb, in_overflow, 4)) return 14;
    if (audio_rb_available_read(&rb) != 6 || audio_rb_available_write(&rb) != 2) return 15;

    const int16_t expected[6] = { 50, 60, 70, 80, 90, 100 };
    for (int i = 0; i < 6; i++) {
        if (!audio_rb_pop_sample(&rb, &sample) || sample != expected[i]) return 20 + i;
    }
    if (audio_rb_available_read(&rb) != 0 || audio_rb_available_write(&rb) != 8) return 26;

    // 6. Reset clears counters and indices
    audio_rb_reset(&rb);
    if (audio_rb_get_overflow_count(&rb) != 0 || audio_rb_get_underflow_count(&rb) != 0) return 27;

    return 0;
}
