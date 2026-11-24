/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "cyd_c5.h"
#define SCOUT_SPEC_X 12
#define SCOUT_SPEC_Y 38
#define SCOUT_SPEC_W (CYD_W - 24)
#define SCOUT_SPEC_H 27
#define SCOUT_VIDEO_W cyd_width()
#define SCOUT_MAX_VIDEO_H 180
#define SCOUT_VIDEO_H (cyd_landscape() ? 118 : SCOUT_MAX_VIDEO_H)
#define SCOUT_VIDEO_X 0
#define SCOUT_VIDEO_Y (cyd_landscape() ? 82 : 100)
#define SCOUT_BUTTON_Y (cyd_height() - 36)
#define SCOUT_BUTTON_H 32
#define SCOUT_ARROW_W(landscape) ((landscape) ? 32 : 24)
#define SCOUT_RESCAN_Y 44
#define SCOUT_SETTING_H 36
#define SCOUT_ORIENTATION_Y 86
#define SCOUT_WALLPAPER_Y 150
#define SCOUT_CALIBRATE_Y 190
#define SCOUT_TEST_LED_PORTRAIT_Y 232
#define SCOUT_MENU_CARD_H (landscape ? 25 : 34)
#define SCOUT_MENU_ROW(i) ((landscape ? 47 : 65) + (i) * (landscape ? 29 : 39))
#define SCOUT_MENU_SCAN_Y SCOUT_MENU_ROW(0)
#define SCOUT_MENU_CONTROL_Y SCOUT_MENU_ROW(1)
#define SCOUT_MENU_SPECTRUM_Y SCOUT_MENU_ROW(2)
#define SCOUT_MENU_SETTINGS_Y SCOUT_MENU_ROW(3)
#define SCOUT_MENU_REMOTE_ID_Y SCOUT_MENU_ROW(4)
#define SCOUT_MENU_BENCH_Y SCOUT_MENU_ROW(5)
#define SCOUT_LOG_BUTTON_X(width) ((width) - 112)
#define SCOUT_LOG_BUTTON_Y 8
#define SCOUT_LOG_BUTTON_W 100
#define SCOUT_LOG_BUTTON_H 28
#define SCOUT_LOG_ROW_Y 66
#define SCOUT_LOG_ROW_H 36
#define SCOUT_LOG_ROWS(height) (((height) - SCOUT_LOG_ROW_Y - 52) / SCOUT_LOG_ROW_H)
