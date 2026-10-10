#include "query_button.h"
#include "config.h"
#include "hardware/gpio.h"
#include "pico/time.h"

static bool btn_is_down = false;
static bool btn_prev_raw = true;
static absolute_time_t last_edge_time;
static absolute_time_t press_start_time;

static bool pressed_event = false;
static uint32_t released_duration = 0;

void query_button_init(void) {
    gpio_init(PIN_QUERY_BTN);
    gpio_set_dir(PIN_QUERY_BTN, GPIO_IN);
    gpio_pull_up(PIN_QUERY_BTN);

    // Debounce a boot-time hold just like a later press.
    btn_prev_raw = gpio_get(PIN_QUERY_BTN);
    btn_is_down = false;
    last_edge_time = get_absolute_time();
    press_start_time = get_absolute_time();
    pressed_event = false;
    released_duration = 0;
}

void query_button_update(void) {
    bool raw = gpio_get(PIN_QUERY_BTN); // Active-low: 0=pressed, 1=released

    if (raw != btn_prev_raw) {
        btn_prev_raw = raw;
        last_edge_time = get_absolute_time();
    }

    // Require both press and release to remain stable for 30 ms.
    if (raw == btn_is_down &&
        absolute_time_diff_us(last_edge_time, get_absolute_time()) >= 30000) {
        if (!raw) {
            pressed_event = true;
            btn_is_down = true;
            press_start_time = get_absolute_time();
        } else {
            btn_is_down = false;
            int64_t held_us = absolute_time_diff_us(press_start_time, get_absolute_time());
            released_duration = (held_us >= 1000) ? (uint32_t)(held_us / 1000) : 1;
        }
    }
}

bool query_button_is_held(void) {
    return btn_is_down;
}

bool query_button_just_pressed(void) {
    if (pressed_event) {
        pressed_event = false;
        return true;
    }
    return false;
}

uint32_t query_button_just_released(void) {
    uint32_t dur = released_duration;
    released_duration = 0;
    return dur;
}

uint32_t query_button_current_hold_ms(void) {
    if (!btn_is_down) {
        return 0;
    }
    int64_t diff = absolute_time_diff_us(press_start_time, get_absolute_time());
    return (diff > 0) ? (uint32_t)(diff / 1000) : 0;
}

bool query_button_get_raw(void) {
    return gpio_get(PIN_QUERY_BTN);
}
