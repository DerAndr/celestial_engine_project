#include "encoder.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"

// State tracking for quadrature signals
static volatile uint8_t last_quad_state = 0;
static volatile int8_t sub_step_acc = 0;
static volatile int32_t net_delta = 0;
static volatile int32_t absolute_pos = 0;

// State tracking for push switch
static bool sw_prev_raw = true;
static absolute_time_t last_sw_edge_time;
static bool sw_clicked_event = false;

// Quadrature decoding transition table:
// Index = (previous_state << 2) | current_state
// Bit 1: Channel A, Bit 0: Channel B
static const int8_t quad_table[16] = {
     0, -1,  1,  0,   // 00 -> 00, 01, 10, 11
     1,  0,  0, -1,   // 01 -> 00, 01, 10, 11
    -1,  0,  0,  1,   // 10 -> 00, 01, 10, 11
     0,  1, -1,  0    // 11 -> 00, 01, 10, 11
};

static void encoder_gpio_irq(uint gpio, uint32_t events);

void encoder_init(void) {
    // Channel A
    gpio_init(PIN_ENCODER_A);
    gpio_set_dir(PIN_ENCODER_A, GPIO_IN);
    gpio_pull_up(PIN_ENCODER_A);

    // Channel B
    gpio_init(PIN_ENCODER_B);
    gpio_set_dir(PIN_ENCODER_B, GPIO_IN);
    gpio_pull_up(PIN_ENCODER_B);

    // Push Switch
    gpio_init(PIN_ENCODER_SW);
    gpio_set_dir(PIN_ENCODER_SW, GPIO_IN);
    gpio_pull_up(PIN_ENCODER_SW);

    // Capture initial states
    uint8_t a = gpio_get(PIN_ENCODER_A) ? 1 : 0;
    uint8_t b = gpio_get(PIN_ENCODER_B) ? 1 : 0;
    last_quad_state = (a << 1) | b;
    sub_step_acc = 0;
    net_delta = 0;
    absolute_pos = 0;

    sw_prev_raw = gpio_get(PIN_ENCODER_SW);
    last_sw_edge_time = get_absolute_time();
    sw_clicked_event = false;

    // Capture rotation even while main is transferring an OLED frame.
    uint32_t edges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
    gpio_set_irq_enabled_with_callback(PIN_ENCODER_A, edges, true, encoder_gpio_irq);
    gpio_set_irq_enabled(PIN_ENCODER_B, edges, true);
}

// Called from IRQ, or from polling with interrupts disabled. Both paths must
// sample/update the same state atomically; a stale polling snapshot can invent edges.
static void encoder_decode_pins(uint32_t pins) {
    uint8_t a = (pins >> PIN_ENCODER_A) & 1u;
    uint8_t b = (pins >> PIN_ENCODER_B) & 1u;
    uint8_t curr_quad = (a << 1) | b;

    if (curr_quad != last_quad_state) {
        uint8_t index = (last_quad_state << 2) | curr_quad;
        int8_t step = quad_table[index];
        if (step == 0) sub_step_acc = 0; // Both channels changed: discard an incomplete cycle.
        sub_step_acc += step;

        // A half cycle must not produce a second menu step for one physical detent.
        if (sub_step_acc >= ENCODER_TRANSITIONS_PER_STEP) {
            net_delta++;
            absolute_pos++;
            sub_step_acc = 0;
        } else if (sub_step_acc <= -ENCODER_TRANSITIONS_PER_STEP) {
            net_delta--;
            absolute_pos--;
            sub_step_acc = 0;
        }

        last_quad_state = curr_quad;
    }
}

static void encoder_gpio_irq(uint gpio, uint32_t events) {
    (void)events;
    if (gpio != PIN_ENCODER_A && gpio != PIN_ENCODER_B) return;
    encoder_decode_pins(gpio_get_all());
}

void encoder_update(void) {
    // 1. Quadrature polling fallback in case GPIO interrupt missed
    uint32_t save = save_and_disable_interrupts();
    encoder_decode_pins(gpio_get_all());
    restore_interrupts(save);

    // 2. Read switch with instant falling-edge detection and 100ms lockout debounce
    bool raw_sw = gpio_get(PIN_ENCODER_SW); // 0 when pressed, 1 when released
    if (!raw_sw && sw_prev_raw) {
        // Falling edge (just pressed down)
        if (absolute_time_diff_us(last_sw_edge_time, get_absolute_time()) > 100000) {
            sw_clicked_event = true;
            last_sw_edge_time = get_absolute_time();
        }
    }
    sw_prev_raw = raw_sw;
}

int32_t encoder_get_delta(void) {
    uint32_t save = save_and_disable_interrupts();
    int32_t d = net_delta;
    net_delta = 0;
    restore_interrupts(save);
    return d;
}

int32_t encoder_get_position(void) {
    uint32_t save = save_and_disable_interrupts();
    int32_t pos = absolute_pos;
    restore_interrupts(save);
    return pos;
}

bool encoder_switch_clicked(void) {
    if (sw_clicked_event) {
        sw_clicked_event = false;
        return true;
    }
    return false;
}

bool encoder_switch_is_down(void) {
    return !gpio_get(PIN_ENCODER_SW); // Active-low: true when pressed
}

void encoder_get_raw_pins(bool *a, bool *b, bool *sw) {
    if (a) *a = gpio_get(PIN_ENCODER_A);
    if (b) *b = gpio_get(PIN_ENCODER_B);
    if (sw) *sw = gpio_get(PIN_ENCODER_SW);
}
