/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum { CYD_WALLPAPER_MAIN, CYD_WALLPAPER_ALL, CYD_WALLPAPER_OFF } cyd_wallpaper_mode_t;

/* Run with SPI2 initialized and the other chip selects idle. */
void cyd_wallpaper_init(void);
void cyd_wallpaper_reload(void);
void cyd_wallpaper_next_file(void);
cyd_wallpaper_mode_t cyd_wallpaper_mode(void);
void cyd_wallpaper_next_mode(void);
const char *cyd_wallpaper_status(void);
bool cyd_wallpaper_active(void);
/* Returns true when the displayed frame changes. */
bool cyd_wallpaper_update(int64_t now);
void cyd_wallpaper_row(uint16_t *pixels, int width, int height, int y);
esp_err_t cyd_sd_access(esp_err_t (*action)(const char *, void *), void *context);
