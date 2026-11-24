/* SPDX-License-Identifier: GPL-3.0-or-later
 * RockBase NM-CYD-C5 display + touch driver (see cyd_c5.h). */
#include "cyd_c5.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cyd_font.h"

#ifndef CONFIG_CYD_C5_TOUCH
#define CONFIG_CYD_C5_TOUCH 0            /* Kconfig leaves a disabled bool undefined */
#endif
#ifndef CYD_LCD_HZ
#define CYD_LCD_HZ (40 * 1000 * 1000)
#endif
#define CYD_TP_HZ (2 * 1000 * 1000)
#define CYD_MAX_XFER (CYD_MAX_W * CYD_BAND_LINES * 2)

static spi_device_handle_t lcd_dev, tp_dev;
/* Heap, not BSS: ESPsoup's static sections must end below its RF dump bank. */
static uint16_t *band_buf;
static uint16_t *frame_buf;
static bool frame_open;
static bool bus_up;
static bool bl_on;
static bool display_landscape;

typedef struct { int16_t a0, a1, b0, b1; uint8_t swap, valid; } cyd_cal_t;
static cyd_cal_t cal;

uint16_t *cyd_band(void) { return band_buf; }

static esp_err_t bus_open(void) {
    spi_bus_config_t bus = {
#if CONFIG_CYD_C5_TOUCH
        .miso_io_num = CYD_PIN_MISO,
#else
        .miso_io_num = -1,                  /* GPIO2 stays with its other owner */
#endif
        .sclk_io_num = CYD_PIN_SCK, .mosi_io_num = CYD_PIN_MOSI,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = CYD_MAX_XFER + 16,
    };
    esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (e != ESP_OK) return e;
    spi_device_interface_config_t lcd = {
        .clock_speed_hz = CYD_LCD_HZ, .mode = 0, .spics_io_num = CYD_PIN_LCD_CS, .queue_size = 1,
    };
    if ((e = spi_bus_add_device(SPI2_HOST, &lcd, &lcd_dev)) != ESP_OK) return e;
    spi_device_interface_config_t tp = {
        .clock_speed_hz = CYD_TP_HZ, .mode = 0, .spics_io_num = -1, .queue_size = 1,
    };
    if ((e = spi_bus_add_device(SPI2_HOST, &tp, &tp_dev)) != ESP_OK) return e;
    bus_up = true;
#if !CONFIG_CYD_C5_TOUCH
    /* No MISO: coordinates are unreadable, but a write-only "power down,
     * pen interrupt on" command (PD1:0 = 00) arms TP_IRQ for cyd_touch_down(). */
    spi_transaction_t arm = { .length = 24, .flags = SPI_TRANS_USE_TXDATA, .tx_data = {0x90, 0, 0} };
    gpio_set_level(CYD_PIN_TP_CS, 0);
    spi_device_polling_transmit(tp_dev, &arm);
    gpio_set_level(CYD_PIN_TP_CS, 1);
    gpio_reset_pin(CYD_PIN_TP_IRQ);
    gpio_set_direction(CYD_PIN_TP_IRQ, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CYD_PIN_TP_IRQ, GPIO_PULLUP_ONLY);
#endif
    return ESP_OK;
}

bool cyd_touch_down(void) { return gpio_get_level(CYD_PIN_TP_IRQ) == 0; }

static void plain_out(int pin, int level) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);
}

static void ctl_pins(void) {
    plain_out(CYD_PIN_LCD_DC, 1);
    plain_out(CYD_PIN_TP_CS, 1);
    plain_out(CYD_PIN_SD_CS, 1);
    gpio_set_direction(CYD_PIN_BL, GPIO_MODE_OUTPUT);
}

void cyd_reclaim_pins(void) {
    if (bus_up) {
        spi_bus_remove_device(lcd_dev);
        if (tp_dev) spi_bus_remove_device(tp_dev);
        spi_bus_free(SPI2_HOST);
        bus_up = false;
    }
    ctl_pins();
    /* An output pad without input enable always reads 0: restore the
     * remembered state, never gpio_get_level (that switched the light off). */
    gpio_set_level(CYD_PIN_BL, bl_on);
    bus_open();
}

static void lcd_cmd(uint8_t c, const uint8_t *data, int n) {
    spi_transaction_t t = { .length = 8, .flags = SPI_TRANS_USE_TXDATA, .tx_data = {c} };
    gpio_set_level(CYD_PIN_LCD_DC, 0);
    spi_device_polling_transmit(lcd_dev, &t);
    gpio_set_level(CYD_PIN_LCD_DC, 1);
    if (n > 0) {
        spi_transaction_t d = { .length = 8 * n, .tx_buffer = data };
        spi_device_polling_transmit(lcd_dev, &d);
    }
}

bool cyd_landscape(void) { return display_landscape; }

static void lcd_orientation(void) {
    const uint8_t madctl = display_landscape ? 0xA0 : 0x00;
    lcd_cmd(0x36, &madctl, 1);
}

void cyd_set_landscape(bool landscape) {
    if (display_landscape == landscape) return;
    display_landscape = landscape;
    lcd_orientation();
    nvs_handle_t h;
    esp_err_t err = nvs_open("cyd", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "landscape", landscape);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err != ESP_OK) printf("SCOUT display preference save failed: %d\n", (int)err);
}

static void lcd_window(int x, int y, int w, int h) {
    uint8_t ca[4] = { x >> 8, x & 255, (x + w - 1) >> 8, (x + w - 1) & 255 };
    uint8_t ra[4] = { y >> 8, y & 255, (y + h - 1) >> 8, (y + h - 1) & 255 };
    lcd_cmd(0x2A, ca, 4);
    lcd_cmd(0x2B, ra, 4);
    lcd_cmd(0x2C, NULL, 0);
}

static void lcd_data(const void *p, size_t bytes) {
    const uint8_t *b = p;
    while (bytes) {
        size_t n = bytes > CYD_MAX_XFER ? CYD_MAX_XFER : bytes;
        spi_transaction_t d = { .length = 8 * n, .tx_buffer = b };
        spi_device_polling_transmit(lcd_dev, &d);
        b += n; bytes -= n;
    }
}

bool cyd_frame_ready(void) {
    if (!frame_buf)
        frame_buf = heap_caps_malloc(CYD_W * CYD_H * sizeof(*frame_buf),
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return frame_buf != NULL;
}

bool cyd_frame_begin(void) {
    frame_open = cyd_frame_ready();
    return frame_open;
}

static void frame_flush(int from, int to) {
    if (from >= to) return;
    const int width = cyd_width();
    const size_t capacity = CYD_MAX_W * CYD_BAND_LINES;
    const uint16_t *source = frame_buf + from * width;
    size_t remaining = (size_t)(to - from) * width;
    lcd_window(0, from, width, to - from);
    while (remaining) {
        size_t count = remaining < capacity ? remaining : capacity;
        memcpy(band_buf, source, count * sizeof(*source));
        lcd_data(band_buf, count * sizeof(*source));
        source += count;
        remaining -= count;
    }
}

void cyd_frame_end(int skip_from, int skip_to) {
    if (!frame_open) return;
    frame_open = false;
    frame_flush(0, skip_from);
    frame_flush(skip_to, cyd_height());
}

static bool clip(int *x, int *y, int *w, int *h) {
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    int width = cyd_width(), height = cyd_height();
    if (*x + *w > width) *w = width - *x;
    if (*y + *h > height) *h = height - *y;
    return *w > 0 && *h > 0;
}

void cyd_blit(int x, int y, int w, int h, const uint16_t *px) {
    if (w <= 0 || h <= 0) return;
    if (frame_open) {
        int width = cyd_width();
        if (x < 0 || y < 0 || w > width - x || h > cyd_height() - y) return;
        for (int row = 0; row < h; row++)
            memcpy(frame_buf + (y + row) * width + x, px + row * w, (size_t)w * sizeof(*px));
        return;
    }
    lcd_window(x, y, w, h);
    lcd_data(px, (size_t)w * h * 2);
}

void cyd_fill(int x, int y, int w, int h, uint16_t colour) {
    if (!clip(&x, &y, &w, &h)) return;
    if (frame_open) {
        int width = cyd_width();
        for (int row = 0; row < h; row++) {
            uint16_t *dst = frame_buf + (y + row) * width + x;
            for (int col = 0; col < w; col++) dst[col] = colour;
        }
        return;
    }
    unsigned chunk = CYD_MAX_W * CYD_BAND_LINES;
    unsigned total = (unsigned)w * h;
    unsigned fillc = total < chunk ? total : chunk;
    for (unsigned j = 0; j < fillc; j++) band_buf[j] = colour;
    lcd_window(x, y, w, h);
    while (total) {
        unsigned n = total > chunk ? chunk : total;
        lcd_data(band_buf, n * 2);
        total -= n;
    }
}
void cyd_hline(int x, int y, int w, uint16_t c) { cyd_fill(x, y, w, 1, c); }
void cyd_vline(int x, int y, int h, uint16_t c) { cyd_fill(x, y, 1, h, c); }

int cyd_text_width(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }

int cyd_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale) {
    if (scale < 1) scale = 1;
    int cw = 6 * scale, ch = 8 * scale;
    /* One glyph at a time, in strips of rows that fit the band buffer
     * (a scale-4 glyph is 768 px; the buffer can be as small as 320 px). */
    const int strip = (CYD_MAX_W * CYD_BAND_LINES) / cw;
    const int width = cyd_width(), height = cyd_height();
    for (; *s; s++, x += cw) {
        if (x >= width) break;
        if (x + cw > width || y + ch > height) continue;
        unsigned c = (unsigned char)*s;
        const uint8_t *g = (c >= 0x20 && c < 0x7F) ? cyd_font5x7[c - 0x20] : cyd_font5x7['?' - 0x20];
        for (int r0 = 0; r0 < ch; r0 += strip) {
            int rows = ch - r0 < strip ? ch - r0 : strip;
            for (int r = 0; r < rows; r++)
                for (int col = 0; col < cw; col++) {
                    int gc = col / scale, gr = (r0 + r) / scale;
                    bool on = gc < 5 && (g[gc] >> gr) & 1;
                    band_buf[r * cw + col] = on ? fg : bg;
                }
            cyd_blit(x, y + r0, cw, rows, band_buf);
        }
    }
    return x;
}

void cyd_backlight(bool on) { bl_on = on; gpio_set_level(CYD_PIN_BL, on ? 1 : 0); }

bool cyd_boot_pressed(void) {
    gpio_set_direction(CYD_PIN_BOOT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CYD_PIN_BOOT, GPIO_PULLUP_ONLY);
    return gpio_get_level(CYD_PIN_BOOT) == 0;
}

/* ------------------------------------------------------------- touch */
static uint16_t tp_xfer(uint8_t cmd) {
    spi_transaction_t t = { .length = 24, .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
                            .tx_data = {cmd, 0, 0} };
    spi_device_polling_transmit(tp_dev, &t);
    return (uint16_t)(((t.rx_data[1] << 8) | t.rx_data[2]) >> 3);
}
static uint16_t med3(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) b = c;
    return a > b ? a : b;
}
/* Raw 12-bit readings, true when the pressure exceeds the threshold. */
static bool tp_raw(int *rx, int *ry) {
    if (!tp_dev || !CONFIG_CYD_C5_TOUCH) { *rx = *ry = 0; return false; }
    /* The PARLIO capture path may have reset pad 1: make it a CS again. */
    gpio_set_direction(CYD_PIN_TP_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(CYD_PIN_TP_CS, 0);
    int z1 = tp_xfer(0xB1), z2 = tp_xfer(0xC1);
    int z = z1 + 4095 - z2;
    bool down = z1 > 50 && z > 400;
    if (down) {
        uint16_t x0 = tp_xfer(0xD1), x1 = tp_xfer(0xD1), x2 = tp_xfer(0xD1);
        uint16_t y0 = tp_xfer(0x91), y1 = tp_xfer(0x91), y2 = tp_xfer(0x90);
        *rx = med3(x0, x1, x2); *ry = med3(y0, y1, y2);
    } else tp_xfer(0x90);   /* power down, PENIRQ enabled */
    gpio_set_level(CYD_PIN_TP_CS, 1);
    return down;
}

/* Keep landscape calibration in its original coordinate system. */
static void touch_display_point(int *x, int *y) {
    if (display_landscape) {
        *x = cyd_width() - 1 - *x;
        *y = cyd_height() - 1 - *y;
    }
}

bool cyd_touch(int *x, int *y) {
    int rx, ry;
    if (!tp_raw(&rx, &ry)) return false;
    const int width = cyd_width(), height = cyd_height();
    int a = cal.swap ? ry : rx, b = cal.swap ? rx : ry;
    if (!cal.valid || cal.a1 == cal.a0 || cal.b1 == cal.b0) {
        *x = rx * width / 4096; *y = ry * height / 4096;
    } else {
        int sx = 20 + (a - cal.a0) * (width - 40) / (cal.a1 - cal.a0);
        int sy = 20 + (b - cal.b0) * (height - 40) / (cal.b1 - cal.b0);
        *x = sx < 0 ? 0 : sx >= width ? width - 1 : sx;
        *y = sy < 0 ? 0 : sy >= height ? height - 1 : sy;
    }
    touch_display_point(x, y);
    return true;
}

static void cross(int x, int y, uint16_t c) {
    cyd_hline(x - 10, y, 21, c);
    cyd_vline(x, y - 10, 21, c);
}
static void wait_tap(int *rx, int *ry) {
    int dummy;
    while (tp_raw(&dummy, &dummy)) vTaskDelay(pdMS_TO_TICKS(20));      /* release first */
    vTaskDelay(pdMS_TO_TICKS(100));
    long sx = 0, sy = 0; int n = 0;
    for (;;) {
        int x, y;
        if (tp_raw(&x, &y)) { sx += x; sy += y; if (++n >= 12) break; }
        else n = 0, sx = sy = 0;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    *rx = (int)(sx / n); *ry = (int)(sy / n);
    while (tp_raw(&dummy, &dummy)) vTaskDelay(pdMS_TO_TICKS(20));
}

static const char *cal_key(void) { return display_landscape ? "tcal" : "tcal_portrait"; }

static void cal_load(void) {
    nvs_handle_t h;
    size_t len = sizeof(cal);
    memset(&cal, 0, sizeof(cal));
    if (nvs_open("cyd", NVS_READONLY, &h) != ESP_OK) return;
    if (nvs_get_blob(h, cal_key(), &cal, &len) != ESP_OK || len != sizeof(cal)) memset(&cal, 0, sizeof(cal));
    nvs_close(h);
}
static void cal_save(void) {
    nvs_handle_t h;
    if (nvs_open("cyd", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, cal_key(), &cal, sizeof(cal));
    nvs_commit(h);
    nvs_close(h);
}

void cyd_touch_calibrate(bool force) {
    if (!tp_dev || !CONFIG_CYD_C5_TOUCH) return;
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    cal_load();
    if (cal.valid && cal.a1 != cal.a0 && cal.b1 != cal.b0 && !force && !cyd_boot_pressed()) return;
    const int width = cyd_width(), height = cyd_height();
    const int tx[3] = {20, width - 20, 20}, ty[3] = {20, 20, height - 20};
    int rx[3], ry[3];
    cyd_fill(0, 0, width, height, CYD_BLACK);
    cyd_label(24, height / 2 - 41, "Touch calibration", CYD_WHITE, CYD_BLACK, CYD_FONT_TITLE);
    cyd_label(24, height / 2 - 9, "Tap each cross with a stylus", CYD_GREY, CYD_BLACK, CYD_FONT_SMALL);
    for (int k = 0; k < 3; k++) {
        int x = tx[k], y = ty[k];
        touch_display_point(&x, &y);
        cross(x, y, CYD_YELLOW);
        wait_tap(&rx[k], &ry[k]);
        cross(x, y, CYD_BLACK);
    }
    /* Screen x runs from target 0 to 1, screen y from target 0 to 2. */
    int dxx = rx[1] - rx[0], dxy = ry[1] - ry[0];
    cal.swap = (dxy < 0 ? -dxy : dxy) > (dxx < 0 ? -dxx : dxx);
    if (cal.swap) { cal.a0 = ry[0]; cal.a1 = ry[1]; cal.b0 = rx[0]; cal.b1 = rx[2]; }
    else          { cal.a0 = rx[0]; cal.a1 = rx[1]; cal.b0 = ry[0]; cal.b1 = ry[2]; }
    cal.valid = 1;
    cal_save();
    cyd_fill(0, 0, width, height, CYD_BLACK);
}

/* ------------------------------------------------------------- init */
esp_err_t cyd_init(void) { return cyd_init_with_bus_hook(NULL); }

esp_err_t cyd_init_with_bus_hook(void (*setup)(void)) {
    if (!band_buf) band_buf = heap_caps_malloc(CYD_MAX_XFER, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!band_buf) return ESP_ERR_NO_MEM;
    gpio_reset_pin(CYD_PIN_BL);
    ctl_pins();
    gpio_set_level(CYD_PIN_BL, 0);
    esp_err_t e = bus_open();
    if (e != ESP_OK) return e;
    if (setup) setup();
    lcd_cmd(0x01, NULL, 0);                  /* SWRESET (RST is tied to EN) */
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(0x11, NULL, 0);                  /* SLPOUT */
    vTaskDelay(pdMS_TO_TICKS(120));
    nvs_handle_t h;
    if (nvs_open("cyd", NVS_READONLY, &h) == ESP_OK) {
        uint8_t saved = 0;
        display_landscape = nvs_get_u8(h, "landscape", &saved) == ESP_OK && saved == 1;
        nvs_close(h);
    }
    static const uint8_t colmod = 0x55;
    lcd_cmd(0x3A, &colmod, 1);               /* 16 bit/px */
    lcd_orientation();
    lcd_cmd(0x20, NULL, 0);                  /* INVOFF: RockBase panel (TFT_INVERSION_OFF) */
    lcd_cmd(0x13, NULL, 0);                  /* NORON */
    cyd_fill(0, 0, cyd_width(), cyd_height(), CYD_BLACK);
    lcd_cmd(0x29, NULL, 0);                  /* DISPON */
    cyd_backlight(true);
    return ESP_OK;
}
