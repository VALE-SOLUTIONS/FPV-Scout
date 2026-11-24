/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minimal display + touch driver for the RockBase NM-CYD-C5
 * (ESP32-C5-WROOM-1-N16R8, 2.8" ST7789 240x320, XPT2046 resistive touch).
 *
 * Pins (from the RockBase schematic / connections.md):
 *   shared SPI : SCK 6, MOSI 7, MISO 2
 *   LCD        : CS 23, DC 24, RST = chip EN, backlight 25
 *   touch      : CS 1 (driven as a plain GPIO, see cyd_touch_read)
 *   SD card    : CS 10 (held high, unused here)
 *   BOOT button: 28
 *
 * Everything is polled and single-threaded: call it from one task only.
 * Colours are RGB565 already byte-swapped for the wire (use CYD_RGB).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define CYD_W 240
#define CYD_H 320
#define CYD_MAX_W CYD_H

bool cyd_landscape(void);
void cyd_set_landscape(bool landscape);
static inline int cyd_width(void) { return cyd_landscape() ? CYD_H : CYD_W; }
static inline int cyd_height(void) { return cyd_landscape() ? CYD_W : CYD_H; }

#define CYD_PIN_SCK   6
#define CYD_PIN_MOSI  7
#define CYD_PIN_MISO  2
#define CYD_PIN_LCD_CS 23
#define CYD_PIN_LCD_DC 24
#define CYD_PIN_BL    25
#define CYD_PIN_TP_CS 1
#define CYD_PIN_TP_IRQ 3
#define CYD_PIN_SD_CS 10
#define CYD_PIN_BOOT  28

/* Onboard red indicator: 0=off, 1..30 Hz. Test cycles 1/5/30 Hz for one second each. */
esp_err_t cyd_indicator_init(void);
void cyd_indicator_rate(unsigned hz);
void cyd_indicator_test(void);

/* Lines per DMA band in the shared pixel buffer (CYD_MAX_W * CYD_BAND_LINES px). */
#include "sdkconfig.h"
#ifndef CYD_BAND_LINES
#ifdef CONFIG_CYD_C5_BAND_LINES
#define CYD_BAND_LINES CONFIG_CYD_C5_BAND_LINES
#else
#define CYD_BAND_LINES 4
#endif
#endif

#define CYD_RGB(r, g, b) ((uint16_t)((((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)) >> 8) | \
                                     ((((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)) & 0xFF) << 8)))
static inline uint16_t cyd_rgb(unsigned r, unsigned g, unsigned b) {
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

#define CYD_BLACK  CYD_RGB(0, 0, 0)
#define CYD_WHITE  CYD_RGB(255, 255, 255)
#define CYD_GREY   CYD_RGB(90, 90, 90)
#define CYD_DGREY  CYD_RGB(30, 30, 38)
#define CYD_YELLOW CYD_RGB(255, 220, 0)
#define CYD_CYAN   CYD_RGB(0, 220, 255)
#define CYD_GREEN  CYD_RGB(0, 230, 90)
#define CYD_RED    CYD_RGB(255, 40, 40)
#define CYD_BLUE   CYD_RGB(40, 90, 255)

/* SPI bus, LCD, backlight, touch CS. */
esp_err_t cyd_init(void);
/* Run the SD setup after the shared SPI bus opens, before sending LCD commands. */
esp_err_t cyd_init_with_bus_hook(void (*setup)(void));
/* Re-claim the LCD/touch pads after another driver reset or rerouted them. */
void cyd_reclaim_pins(void);

void cyd_backlight(bool on);
/* Shared band buffer: CYD_MAX_W * CYD_BAND_LINES pixels, DMA capable. */
uint16_t *cyd_band(void);
/* Compose controls and an animated background in PSRAM before touching the LCD. */
bool cyd_frame_ready(void);
bool cyd_frame_begin(void);
/* Leave rows [skip_from, skip_to) on the LCD untouched. Use (0, 0) to send all rows. */
void cyd_frame_end(int skip_from, int skip_to);
/* Push w*h pixels (wire order) to the rectangle. */
void cyd_blit(int x, int y, int w, int h, const uint16_t *px);
void cyd_fill(int x, int y, int w, int h, uint16_t colour);
void cyd_hline(int x, int y, int w, uint16_t colour);
void cyd_vline(int x, int y, int h, uint16_t colour);
/* 6x8 cell font scaled by `scale`; returns the x after the text. */
int cyd_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
int cyd_text_width(const char *s, int scale);
/* Proportional Inter text, 4-bit antialiasing, positioned at line top. */
typedef enum { CYD_FONT_SMALL, CYD_FONT_UI, CYD_FONT_TITLE, CYD_FONT_NUMBER } cyd_font_t;
int cyd_label_width(const char *text, cyd_font_t font);
void cyd_label(int x, int y, const char *text, uint16_t fg, uint16_t bg, cyd_font_t font);
/* Blend a label into a streaming video row without clearing its background. */
void cyd_label_overlay_row(uint16_t *pixels, int row_x, int row_y, int width,
                           int x, int y, const char *text, uint16_t fg, cyd_font_t font);
/* Hardware vertical scroll is not used: rotation makes it horizontal. */

/* Pen interrupt only (GPIO3): true while the screen is pressed anywhere.
 * Works without MISO (CONFIG_CYD_C5_TOUCH=n arms it at init). */
bool cyd_touch_down(void);
/* Touch: true while pressed, with calibrated screen coordinates. */
bool cyd_touch(int *x, int *y);
/* Three-target calibration, stored in NVS (namespace "cyd").  Runs when no
 * calibration is stored, when `force`, or when BOOT is held at the call. */
void cyd_touch_calibrate(bool force);
bool cyd_boot_pressed(void);
