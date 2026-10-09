#ifndef CELESTIAL_SH1106_H
#define CELESTIAL_SH1106_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

#define SH1106_WIDTH   128
#define SH1106_HEIGHT  64
#define SH1106_PAGES   8
#define SH1106_BUF_SIZE (SH1106_WIDTH * SH1106_PAGES) // 1024 bytes

// Driver API
int  sh1106_i2c_scan(i2c_inst_t *i2c);
bool sh1106_init(i2c_inst_t *i2c, uint8_t addr);
void sh1106_clear(void);
void sh1106_update(void);

// Drawing primitives
void sh1106_draw_pixel(int x, int y, bool on);
void sh1106_draw_line(int x0, int y0, int x1, int y1, bool on);
void sh1106_draw_rect(int x, int y, int w, int h, bool on);
void sh1106_fill_rect(int x, int y, int w, int h, bool on);
void sh1106_draw_char(int x, int y, char c, bool on);
void sh1106_draw_string(int x, int y, const char *str, bool on);
void sh1106_draw_char_scaled(int x, int y, char c, int scale, bool on);
void sh1106_draw_string_scaled(int x, int y, const char *str, int scale, bool on);

#endif // CELESTIAL_SH1106_H
