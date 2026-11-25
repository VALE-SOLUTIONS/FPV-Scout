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
#define SCOUT_SETTING_H 32
#define SCOUT_ORIENTATION_Y 82
#define SCOUT_SIGNAL_CAL_Y 118
#define SCOUT_WALLPAPER_Y 154
#define SCOUT_CALIBRATE_Y 190
#define SCOUT_TEST_LED_PORTRAIT_Y 232
#define SCOUT_ALL_ROW_Y 55
#define SCOUT_ALL_ROW_H(landscape) ((landscape) ? 30 : 46)
#define SCOUT_ALL_ROW_PITCH(landscape) (SCOUT_ALL_ROW_H(landscape) + ((landscape) ? 4 : 6))
#define SCOUT_MENU_TOOL_Y(landscape) ((landscape) ? 192 : 264)
#define SCOUT_MENU_TOOL_H(landscape) ((landscape) ? 20 : 24)
#define SCOUT_MENU_TOOL_PITCH(landscape) (SCOUT_MENU_TOOL_H(landscape) + 4)
#define SCOUT_START_PAGE_Y(landscape) ((landscape) ? SCOUT_RESCAN_Y : 270)
#define SCOUT_CAL_ROW_Y 48
#define SCOUT_CAL_ROW_H 32
#define SCOUT_CAL_ROW_PITCH 37
#define SCOUT_LOG_ROW_Y 66
#define SCOUT_LOG_ROW_H 36
#define SCOUT_LOG_ROWS(height) (((height) - SCOUT_LOG_ROW_Y - 52) / SCOUT_LOG_ROW_H)
