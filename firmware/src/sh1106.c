#include "sh1106.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static i2c_inst_t *driver_i2c = NULL;
static uint8_t driver_addr = 0x3C;
static uint8_t display_buffer[SH1106_BUF_SIZE];
#define I2C_TIMEOUT_US 10000

static sh1106_yield_fn_t page_yield_callback = NULL;

void sh1106_set_page_yield_callback(sh1106_yield_fn_t cb) {
    page_yield_callback = cb;
}

// Command packet sender (Co = 0, D/C# = 0)
static bool send_cmd(uint8_t cmd) {
    uint8_t pkt[2] = {0x00, cmd};
    int ret = i2c_write_timeout_us(driver_i2c, driver_addr, pkt, 2, false, I2C_TIMEOUT_US);
    return (ret == 2);
}

int sh1106_i2c_scan(i2c_inst_t *i2c) {
    int found_addr = -1;
    printf("[I2C] Probing SH1106 addresses 0x3C/0x3D...\n");
    for (uint8_t addr = 0x3C; addr <= 0x3D; addr++) {
        uint8_t rxdata;
        // Perform 1-byte read test with 10ms timeout
        int ret = i2c_read_timeout_us(i2c, addr, &rxdata, 1, false, 10000);
        if (ret == 1) {
            printf("[I2C]   -> Found device at address 0x%02X\n", addr);
            if (found_addr < 0) found_addr = addr;
        }
    }
    if (found_addr < 0) {
        printf("[I2C]   -> No devices responded on I2C bus.\n");
    }
    return found_addr;
}

bool sh1106_init(i2c_inst_t *i2c, uint8_t addr) {
    driver_i2c = i2c;
    driver_addr = addr;

    // SH1106 128x64 display initialization sequence
    const uint8_t init_cmds[] = {
        0xAE,       // Display OFF
        0x02,       // Set lower column address
        0x10,       // Set higher column address
        0x40,       // Set display start line to 0
        0xB0,       // Set page address
        0x81, 0x80, // Contrast control: 0x80 (mid-brightness)
        0xA1,       // Set segment re-map: A1 (left/right normal)
        0xA6,       // Normal display (non-inverted)
        0xA8, 0x3F, // Multiplex ratio: 1/64 duty (64MUX)
        0xAD, 0x8B, // DC-DC charge pump control: ON
        0xC8,       // COM output scan direction: remapped
        0xD3, 0x00, // Display offset: 0
        0xD5, 0x80, // Display clock divide ratio/oscillator: default
        0xD9, 0x1F, // Pre-charge period: 0x1F
        0xDA, 0x12, // COM pins hardware config: alternative
        0xDB, 0x40, // VCOM deselect level: 0x40
        0xAF        // Display ON
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        if (!send_cmd(init_cmds[i])) {
            return false;
        }
    }

    sh1106_clear();
    return sh1106_update();
}

void sh1106_clear(void) {
    memset(display_buffer, 0x00, SH1106_BUF_SIZE);
}

bool sh1106_update(void) {
    if (!driver_i2c) return false;

    for (uint8_t p = 0; p < SH1106_PAGES; p++) {
        // Set page and column address with SH1106 2-column offset
        if (!send_cmd(0xB0 | p) || !send_cmd(0x02) || !send_cmd(0x10)) {
            printf("[OLED] Command transfer failed at page %u.\n", p);
            return false;
        }

        // Send 128 bytes of data for this page
        uint8_t payload[129];
        payload[0] = 0x40; // Co = 0, D/C# = 1 (Data mode)
        memcpy(&payload[1], &display_buffer[p * SH1106_WIDTH], SH1106_WIDTH);
        if (i2c_write_timeout_us(driver_i2c, driver_addr, payload, sizeof(payload), false,
                                 I2C_TIMEOUT_US) != (int)sizeof(payload)) {
            printf("[OLED] Frame transfer failed at page %u.\n", p);
            return false;
        }
        if (page_yield_callback) {
            page_yield_callback();
        }
    }
    return true;
}

void sh1106_draw_pixel(int x, int y, bool on) {
    if (x < 0 || x >= SH1106_WIDTH || y < 0 || y >= SH1106_HEIGHT) {
        return;
    }
    int page = y / 8;
    int bit  = y % 8;
    int idx  = (page * SH1106_WIDTH) + x;

    if (on) {
        display_buffer[idx] |= (1 << bit);
    } else {
        display_buffer[idx] &= ~(1 << bit);
    }
}

void sh1106_draw_line(int x0, int y0, int x1, int y1, bool on) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        sh1106_draw_pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void sh1106_draw_rect(int x, int y, int w, int h, bool on) {
    for (int i = x; i < x + w; i++) {
        sh1106_draw_pixel(i, y, on);
        sh1106_draw_pixel(i, y + h - 1, on);
    }
    for (int j = y; j < y + h; j++) {
        sh1106_draw_pixel(x, j, on);
        sh1106_draw_pixel(x + w - 1, j, on);
    }
}

void sh1106_fill_rect(int x, int y, int w, int h, bool on) {
    for (int i = x; i < x + w; i++) {
        for (int j = y; j < y + h; j++) {
            sh1106_draw_pixel(i, j, on);
        }
    }
}

void sh1106_draw_char(int x, int y, char c, bool on) {
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                sh1106_draw_pixel(x + col, y + row, on);
            }
        }
    }
}

/* A response owns the entire OLED; clip partly visible glyphs at both edges. */
void sh1106_draw_response_marquee(const char *text, uint32_t elapsed_ms) {
    if (!text || !*text) return;
    const int scale = 8, step = 48;
    int width = (int)strlen(text) * step;
    int cycle = width + SH1106_WIDTH;
    int offset = (int)((elapsed_ms / 12u) % (uint32_t)cycle);
    for (int repeat = 0; repeat < 2; ++repeat) {
        int base = -offset + repeat * cycle;
        for (size_t i = 0; text[i]; ++i) {
            int x = base + (int)i * step;
            if (x < SH1106_WIDTH && x + 5 * scale > 0)
                sh1106_draw_char_scaled(x, 4, text[i], scale, true);
        }
    }
}

void sh1106_draw_string(int x, int y, const char *str, bool on) {
    int cur_x = x;
    while (*str) {
        if (cur_x + 6 > SH1106_WIDTH) break;
        sh1106_draw_char(cur_x, y, *str++, on);
        cur_x += 6; // 5 pixels width + 1 pixel inter-character space
    }
}

void sh1106_draw_char_scaled(int x, int y, char c, int scale, bool on) {
    if (scale <= 1) {
        sh1106_draw_char(x, y, c, on);
        return;
    }
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                sh1106_fill_rect(x + col * scale, y + row * scale, scale, scale, on);
            }
        }
    }
}

void sh1106_draw_string_scaled(int x, int y, const char *str, int scale, bool on) {
    if (scale <= 1) {
        sh1106_draw_string(x, y, str, on);
        return;
    }
    int cur_x = x;
    int char_step = 5 * scale + scale; // 5 columns + 1 space column
    while (*str) {
        if (cur_x + 5 * scale > SH1106_WIDTH) break;
        sh1106_draw_char_scaled(cur_x, y, *str++, scale, on);
        cur_x += char_step;
    }
}

void sh1106_draw_bitmap(int x, int y, int w, int h, const uint8_t *bitmap, bool on) {
    if (bitmap == NULL) return;
    int bytes_per_row = (w + 7) / 8;
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            uint8_t byte = bitmap[r * bytes_per_row + (c / 8)];
            if (byte & (0x80 >> (c % 8))) {
                sh1106_draw_pixel(x + c, y + r, on);
            }
        }
    }
}
