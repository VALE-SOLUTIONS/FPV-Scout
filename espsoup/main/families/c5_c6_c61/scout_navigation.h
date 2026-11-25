/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "scout_layout.h"
enum { SCOUT_MENU, SCOUT_SCAN_PAGE, SCOUT_SETTINGS, SCOUT_BENCH, SCOUT_SPECTRUM, SCOUT_CONTROL, SCOUT_REMOTE_ID, SCOUT_SIGNAL_LOG, SCOUT_SIGNAL_CAL };
enum { SCOUT_NAV_NONE, SCOUT_NAV_MENU, SCOUT_NAV_SCAN, SCOUT_NAV_SETTINGS,
       SCOUT_NAV_CALIBRATE, SCOUT_NAV_LEFT, SCOUT_NAV_RIGHT, SCOUT_NAV_LED_TEST,
       SCOUT_NAV_BENCH, SCOUT_NAV_ORIENTATION, SCOUT_NAV_WALLPAPER, SCOUT_NAV_WALLPAPER_MODE,
       SCOUT_NAV_PREVIOUS, SCOUT_NAV_CHANNEL_CENTER, SCOUT_NAV_AUTO_RESCAN, SCOUT_NAV_SPECTRUM,
       SCOUT_NAV_SCAN_DISPLAY, SCOUT_NAV_CONTROL, SCOUT_NAV_REMOTE_ID, SCOUT_NAV_SIGNAL_LOG,
       SCOUT_NAV_HOME_ANALOG, SCOUT_NAV_START_PAGE, SCOUT_NAV_HOME_DIGITAL, SCOUT_NAV_SIGNAL_CAL };
#ifdef SCOUT_RUST_CORE
extern int rust_scout_navigation(unsigned,int,int,bool);
#define scout_navigation rust_scout_navigation
#else
static int scout_navigation(unsigned page,int x,int y,bool landscape) {
    const int w = landscape ? CYD_H : CYD_W, h = landscape ? CYD_W : CYD_H;
    if(x<0 || y<0 || x>=w || y>=h)return SCOUT_NAV_NONE;
    bool card = x>=16 && x<w-16;
    if(page==SCOUT_MENU) {
        if(!card) return SCOUT_NAV_NONE;
        const int row = y - SCOUT_ALL_ROW_Y, pitch = SCOUT_ALL_ROW_PITCH(landscape);
        if (row >= 0 && row < 4 * pitch && row % pitch < SCOUT_ALL_ROW_H(landscape)) {
            const int actions[] = {SCOUT_NAV_HOME_ANALOG, SCOUT_NAV_HOME_DIGITAL, SCOUT_NAV_CONTROL, SCOUT_NAV_REMOTE_ID};
            return actions[row / pitch];
        }
        const int tool = y - SCOUT_MENU_TOOL_Y(landscape), tool_pitch = SCOUT_MENU_TOOL_PITCH(landscape);
        if (tool >= 0 && tool < 2 * tool_pitch && tool % tool_pitch < SCOUT_MENU_TOOL_H(landscape)) {
            const bool second = tool >= tool_pitch;
            if (x < w/2-6) return second ? SCOUT_NAV_SIGNAL_LOG : SCOUT_NAV_SPECTRUM;
            if (x >= w/2+6) return second ? SCOUT_NAV_BENCH : SCOUT_NAV_SETTINGS;
        }
    } else {
        if(x>=8 && x<66 && y>=6 && y<34)return SCOUT_NAV_MENU;
        if(page==SCOUT_SCAN_PAGE && x>=SCOUT_SPEC_X && x<w-SCOUT_SPEC_X &&
           y>=SCOUT_SPEC_Y && y<SCOUT_SPEC_Y+SCOUT_SPEC_H)return SCOUT_NAV_SCAN_DISPLAY;
        if(page==SCOUT_SETTINGS && card) {
            if(y>=SCOUT_START_PAGE_Y(landscape) && y<SCOUT_START_PAGE_Y(landscape)+SCOUT_SETTING_H &&
               (!landscape || x>=w/2+6)) return SCOUT_NAV_START_PAGE;
            if(y>=SCOUT_RESCAN_Y && y<SCOUT_RESCAN_Y+SCOUT_SETTING_H &&
               (!landscape || x<w/2-6)) return SCOUT_NAV_AUTO_RESCAN;
            if(y>=SCOUT_ORIENTATION_Y && y<SCOUT_ORIENTATION_Y+SCOUT_SETTING_H)return SCOUT_NAV_ORIENTATION;
            if(y>=SCOUT_SIGNAL_CAL_Y && y<SCOUT_SIGNAL_CAL_Y+SCOUT_SETTING_H)return SCOUT_NAV_SIGNAL_CAL;
            if(y>=SCOUT_WALLPAPER_Y && y<SCOUT_WALLPAPER_Y+SCOUT_SETTING_H) {
                if(x<w/2-6)return SCOUT_NAV_WALLPAPER_MODE;
                if(x>=w/2+6)return SCOUT_NAV_WALLPAPER;
            }
            if(y>=SCOUT_CALIBRATE_Y && y<SCOUT_CALIBRATE_Y+SCOUT_SETTING_H) {
                if(!landscape || x<w/2-6)return SCOUT_NAV_CALIBRATE;
                if(x>=w/2+6)return SCOUT_NAV_LED_TEST;
            }
            if(!landscape && y>=SCOUT_TEST_LED_PORTRAIT_Y &&
               y<SCOUT_TEST_LED_PORTRAIT_Y+SCOUT_SETTING_H)return SCOUT_NAV_LED_TEST;
        }
        if((page==SCOUT_SCAN_PAGE || page==SCOUT_BENCH || page==SCOUT_SPECTRUM || page==SCOUT_SIGNAL_LOG) && y>=h-36 && y<h-4) {
            if(x>=12 && x<w/2-6)return SCOUT_NAV_LEFT;
            if(x>=w/2+6 && x<w-12) {
                if (page == SCOUT_SCAN_PAGE) {
                    int arrow = SCOUT_ARROW_W(landscape);
                    if (x < w/2+6+arrow) return SCOUT_NAV_PREVIOUS;
                    if (x < w-12-arrow) return SCOUT_NAV_CHANNEL_CENTER;
                }
                return SCOUT_NAV_RIGHT;
            }
        }
    }
    return SCOUT_NAV_NONE;
}

#endif
