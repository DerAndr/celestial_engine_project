#ifndef TEST_PICO_STUBS_H
#define TEST_PICO_STUBS_H
/* Host substitutes for hardware only; tests execute the production C drivers. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <setjmp.h>
typedef unsigned int uint;
typedef int64_t absolute_time_t;
static int64_t test_now;
static bool test_gpio[48];
static int interrupt_depth;
static absolute_time_t get_absolute_time(void) { return test_now; }
static int64_t absolute_time_diff_us(absolute_time_t a, absolute_time_t b) { return b-a; }
static absolute_time_t delayed_by_ms(absolute_time_t t, uint32_t ms) { return t + (int64_t)ms*1000; }
static inline uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000); }
static uint32_t save_and_disable_interrupts(void) { return interrupt_depth++; }
static void restore_interrupts(uint32_t n) { interrupt_depth = n; }
#define __compiler_memory_barrier() do {} while (0)
#define GPIO_IN 0
#define GPIO_OUT 1
#define GPIO_FUNC_I2C 2
static void gpio_init(uint pin) { (void)pin; }
static void gpio_set_dir(uint pin, bool out) { (void)pin; (void)out; }
static void gpio_pull_up(uint pin) { (void)pin; }
static bool gpio_get(uint pin) { return test_gpio[pin]; }
static void gpio_put(uint pin, bool value) { test_gpio[pin] = value; }
static void gpio_set_function(uint pin, uint fn) { (void)pin; (void)fn; }
typedef void (*test_gpio_irq_t)(uint, uint32_t);
static test_gpio_irq_t test_gpio_irq;
static uint32_t test_gpio_irq_mask[48];
#define GPIO_IRQ_EDGE_RISE 8
#define GPIO_IRQ_EDGE_FALL 4
static uint32_t gpio_get_all(void) { uint32_t pins=0; for(uint i=0;i<32;i++) if(test_gpio[i]) pins |= 1u<<i; return pins; }
static void gpio_set_irq_enabled(uint pin, uint32_t mask, bool enabled) { test_gpio_irq_mask[pin]=enabled ? mask : 0; }
static void gpio_set_irq_enabled_with_callback(uint pin, uint32_t mask, bool enabled, test_gpio_irq_t callback) { test_gpio_irq=callback; gpio_set_irq_enabled(pin,mask,enabled); }
static void test_gpio_transition(uint pin, bool level) {
    bool before=test_gpio[pin]; test_gpio[pin]=level;
    uint32_t event=level ? GPIO_IRQ_EDGE_RISE : GPIO_IRQ_EDGE_FALL;
    if(before != level && test_gpio_irq && (test_gpio_irq_mask[pin]&event)) test_gpio_irq(pin,event);
}
typedef struct { uint32_t txf[4], rxf[4], fdebug; } test_pio_t;
#define PIO_FDEBUG_TXSTALL_LSB 24
static test_pio_t test_pios[3];
typedef test_pio_t *PIO;
#define pio0 (&test_pios[0])
#define pio1 (&test_pios[1])
#define pio2 (&test_pios[2])
static bool test_sm_enabled[3][4];
static uint test_fifo_count, accepted_pixels;
static uint pio_add_program(PIO p, const void *program) { (void)p; (void)program; return 0; }
static void pio_sm_claim(PIO p, uint sm) { (void)p; (void)sm; }
static void pio_sm_restart(PIO p, uint sm) { (void)p; (void)sm; }
static void pio_sm_clkdiv_restart(PIO p, uint sm) { (void)p; (void)sm; }
static void pio_sm_clear_fifos(PIO p, uint sm) { (void)p; (void)sm; }
static void pio_sm_set_pins_with_mask(PIO p, uint sm, uint32_t values, uint32_t mask) { (void)p; (void)sm; for(uint i=0;i<32;i++) if(mask & (1u<<i)) test_gpio[i]=(values & (1u<<i)) != 0; }
static void pio_sm_exec(PIO p, uint sm, uint instr) { (void)p; (void)sm; (void)instr; }
static uint pio_encode_jmp(uint addr) { return addr; }
static void dma_channel_abort(uint ch) { (void)ch; }
static void pio_sm_set_enabled(PIO p, uint sm, bool enabled) { test_sm_enabled[p-test_pios][sm] = enabled; }
static void pio_sm_set_clkdiv(PIO p, uint sm, float div) { (void)p; (void)sm; (void)div; }
static void pio_gpio_init(PIO p, uint pin) { (void)p; (void)pin; }
static uint pio_get_dreq(PIO p, uint sm, bool tx) { return (uint)(p-test_pios)*8 + sm + (tx ? 0 : 4); }
static void pio_sm_put(PIO p, uint sm, uint32_t data) {
    (void)p; (void)sm; (void)data;
    if (test_fifo_count < 8) { test_fifo_count++; accepted_pixels++; }
}
static void pio_sm_put_blocking(PIO p, uint sm, uint32_t data) {
    if (test_fifo_count == 8) { test_fifo_count--; test_now += 30; }
    pio_sm_put(p, sm, data);
}
static bool pio_sm_is_tx_fifo_empty(PIO p, uint sm) {
    (void)p; (void)sm;
    if (test_fifo_count) { test_fifo_count--; test_now += 30; return false; }
    return true;
}
static void tight_loop_contents(void) {}
static uint64_t test_last_sleep_us;
static void sleep_us(uint64_t us) {
    test_last_sleep_us = us;
    test_now += us;
    uint consumed = (uint)(us/30);
    test_fifo_count = consumed < test_fifo_count ? test_fifo_count-consumed : 0;
}
static jmp_buf main_exit;
static int main_scenario, main_loop_count;
static void test_main_tick(void);
static void sleep_ms(uint32_t ms) {
    sleep_us((uint64_t)ms*1000);
    if (main_scenario && ms == 2) test_main_tick();
}
static void stdio_init_all(void) {}
static const int audio_i2s_program, mic_i2s_program, ws2812_program;
static const uint audio_i2s_offset_entry_point = 0;
static void audio_i2s_program_init(PIO p, uint sm, uint o, uint d, uint c) { (void)p; (void)sm; (void)o; (void)d; (void)c; }
static void mic_i2s_program_init(PIO p, uint sm, uint o, uint d, uint c) { (void)p; (void)sm; (void)o; (void)d; (void)c; }
static void ws2812_program_init(PIO p, uint sm, uint o, uint pin, float freq, bool rgbw) { (void)p; (void)sm; (void)o; (void)pin; (void)freq; (void)rgbw; }
#define clk_sys 0
static uint32_t clock_get_hz(int clk) { (void)clk; return 150000000; }
typedef struct { bool ring_write; uint ring_bits; uint size, dreq; bool read_increment, write_increment, high_priority; } dma_channel_config;
typedef struct { uintptr_t write_addr; uint32_t transfer_count; uint32_t ctrl_trig, al1_ctrl; } test_dma_hw_t;
#define DMA_CH0_CTRL_TRIG_EN_BITS 1u
static void hw_clear_bits(volatile uint32_t *p, uint32_t mask) { *p &= ~mask; }
static void hw_set_bits(volatile uint32_t *p, uint32_t mask) { *p |= mask; }
static test_dma_hw_t test_dma_hw[16];
static void (*test_dma_cursor_hook)(void);
static int next_dma;
static bool test_force_dma_idle;
static uint32_t test_dma_busy_mask = UINT32_MAX;
static dma_channel_config test_dma_configs[16];
static const volatile uint32_t *test_dma_read_ptrs[16];
static uint32_t test_dma_words[256];
static uint test_dma_word_count, test_dma_wait_count;
static uint test_dma_last_channel;
static uint32_t test_dma_irq_flags, test_dma_start_count;
static int dma_claim_unused_channel(bool required) { (void)required; return next_dma++; }
static dma_channel_config dma_channel_get_default_config(uint channel) { (void)channel; dma_channel_config c = {0}; return c; }
#define DMA_SIZE_32 2
static void channel_config_set_transfer_data_size(dma_channel_config *c, uint size) { c->size = size; }
static void channel_config_set_read_increment(dma_channel_config *c, bool b) { c->read_increment = b; }
static void channel_config_set_write_increment(dma_channel_config *c, bool b) { c->write_increment = b; }
static void channel_config_set_dreq(dma_channel_config *c, uint d) { c->dreq = d; }
static void channel_config_set_high_priority(dma_channel_config *c, bool b) { c->high_priority = b; }
static void channel_config_set_chain_to(dma_channel_config *c, uint d) { (void)c; (void)d; }
static void channel_config_set_ring(dma_channel_config *c, bool w, uint n) { c->ring_write = w; c->ring_bits = n; }
static void dma_channel_configure(uint ch, const dma_channel_config *c, volatile void *w, const volatile void *r, uint32_t n, bool start) { test_dma_configs[ch] = *c; test_dma_read_ptrs[ch] = r; test_dma_hw[ch].write_addr = (uintptr_t)w; test_dma_hw[ch].transfer_count = n; if (start) test_dma_start_count++; }
static void dma_channel_wait_for_finish_blocking(uint ch) {
    test_dma_last_channel = ch;
    test_dma_wait_count++;
    test_dma_word_count = test_dma_hw[ch].transfer_count;
    if (test_dma_word_count > 256 || test_dma_hw[ch].write_addr != (uintptr_t)&pio0->txf[1]) abort();
    for (uint i = 0; i < test_dma_word_count; ++i) {
        test_dma_words[i] = test_dma_read_ptrs[ch][i];
        pio_sm_put_blocking(pio0, 1, test_dma_words[i]);
    }
    test_dma_hw[ch].transfer_count = 0;
}
static void dma_channel_set_irq0_enabled(uint ch, bool enabled) { (void)ch; (void)enabled; }
static bool dma_channel_get_irq0_status(uint ch) { return (test_dma_irq_flags & (1u << ch)) != 0; }
static void dma_channel_acknowledge_irq0(uint ch) { test_dma_irq_flags &= ~(1u << ch); }
static void dma_channel_set_trans_count(uint ch, uint32_t n, bool start) { (void)ch; (void)n; (void)start; }
static void dma_channel_set_read_addr(uint ch, const void *p, bool start) { (void)ch; (void)p; (void)start; }
static void dma_channel_start(uint ch) { (void)ch; test_dma_start_count++; }
static bool dma_channel_is_busy(uint ch) { return !test_force_dma_idle && (test_dma_busy_mask & (1u << ch)); }
static test_dma_hw_t *dma_channel_hw_addr(uint ch) { if (test_dma_cursor_hook) test_dma_cursor_hook(); return &test_dma_hw[ch]; }
static uint32_t dma_encode_endless_transfer_count(void) { return 0xffffffffu; }
#define DMA_IRQ_0 0
static void irq_set_exclusive_handler(uint irq, void (*handler)(void)) { (void)irq; (void)handler; }
static void irq_set_enabled(uint irq, bool b) { (void)irq; (void)b; }
typedef struct { int unused; } i2c_inst_t;
static i2c_inst_t test_i2c;
#define i2c1 (&test_i2c)
static int blocking_i2c_calls, i2c_fail_payload, i2c_fail_command;
static int test_i2c_write(const uint8_t *p, size_t n) { return (i2c_fail_command || (i2c_fail_payload && p[0] == 0x40)) ? -1 : (int)n; }
static int i2c_write_blocking(i2c_inst_t *i, uint8_t a, const uint8_t *p, size_t n, bool stop) { (void)i; (void)a; (void)stop; blocking_i2c_calls++; return test_i2c_write(p,n); }
static int i2c_write_timeout_us(i2c_inst_t *i, uint8_t a, const uint8_t *p, size_t n, bool stop, uint timeout) { (void)i; (void)a; (void)stop; (void)timeout; return test_i2c_write(p,n); }
static int i2c_read_timeout_us(i2c_inst_t *i, uint8_t a, uint8_t *p, size_t n, bool stop, uint timeout) { (void)i; (void)p; (void)n; (void)stop; (void)timeout; return (a == 0x20 || a == 0x3c) ? 1 : -1; }
static void i2c_init(i2c_inst_t *i, uint baud) { (void)i; (void)baud; }
#define CYW43_WL_GPIO_LED_PIN 0
static int cyw43_fail, cyw43_uninitialized_calls;
static int cyw43_arch_init(void) { return cyw43_fail; }
static void cyw43_arch_enable_sta_mode(void) { if(cyw43_fail) cyw43_uninitialized_calls++; }
static void cyw43_arch_gpio_put(uint pin, bool b) { (void)pin; (void)b; if(cyw43_fail) cyw43_uninitialized_calls++; }
#define PICO_ERROR_TIMEOUT -1
static int getchar_timeout_us(uint32_t us) { (void)us; return -1; }
static void reset_usb_boot(uint32_t a, uint32_t b) { (void)a; (void)b; }
#define __attribute__(x)
#endif
