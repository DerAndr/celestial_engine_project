#include "query_button.h"
#include "config.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdio.h>

static bool btn_is_down = false;
static bool btn_prev_raw = true;
static absolute_time_t last_edge_time;
static absolute_time_t press_start_time;

static bool pressed_event = false;
static uint32_t released_duration = 0;

static bool sim_active = false;

void query_button_init(void) {
    gpio_init(PIN_QUERY_BTN);
    gpio_set_dir(PIN_QUERY_BTN, GPIO_IN);
    gpio_pull_up(PIN_QUERY_BTN);

    btn_prev_raw = gpio_get(PIN_QUERY_BTN);
    btn_is_down = false;
    sim_active = false;
    last_edge_time = get_absolute_time();
    press_start_time = get_absolute_time();
    pressed_event = false;
    released_duration = 0;
    printf("[QRY_BTN] Initialized GP%d (Pin 12) with pull-up. Initial raw level: %d (%s)\n",
           PIN_QUERY_BTN, btn_prev_raw, btn_prev_raw ? "HIGH/OPEN" : "LOW/PRESSED");
}

void query_button_update(void) {
    if (sim_active) {
        return; // Simulation in progress, do not override from physical pin
    }

    bool raw = gpio_get(PIN_QUERY_BTN); // Active-low: 0=pressed, 1=released
    bool physically_pressed = !raw;

    if (raw != btn_prev_raw) {
        btn_prev_raw = raw;
        last_edge_time = get_absolute_time();
        printf("[QRY_BTN] GP%d edge: raw=%d (%s)\n",
               PIN_QUERY_BTN, raw, raw ? "HIGH/RELEASED" : "LOW/PRESSED");
    }

    // Require both press and release to remain stable for 25 ms.
    if (physically_pressed != btn_is_down &&
        absolute_time_diff_us(last_edge_time, get_absolute_time()) >= 25000) {
        btn_is_down = physically_pressed;
        if (btn_is_down) {
            pressed_event = true;
            press_start_time = get_absolute_time();
            printf("[QRY_BTN] >>> PRESS CONFIRMED (debounced 25ms) <<<\n");
        } else {
            int64_t held_us = absolute_time_diff_us(press_start_time, get_absolute_time());
            released_duration = (held_us >= 1000) ? (uint32_t)(held_us / 1000) : 1;
            printf("[QRY_BTN] >>> RELEASE CONFIRMED (held %lu ms) <<<\n", (unsigned long)released_duration);
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

void query_button_simulate_press(void) {
    sim_active = true;
    pressed_event = true;
    btn_is_down = true;
    press_start_time = get_absolute_time();
    printf("[QRY_BTN] Simulated press injected via serial (sim_active=true)!\n");
}

void query_button_simulate_release(uint32_t duration_ms) {
    sim_active = false;
    btn_is_down = false;
    released_duration = (duration_ms > 0) ? duration_ms : 500;
    printf("[QRY_BTN] Simulated release injected via serial! Duration: %lu ms\n", (unsigned long)released_duration);
}

