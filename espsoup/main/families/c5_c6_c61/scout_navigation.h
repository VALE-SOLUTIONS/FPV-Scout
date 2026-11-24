/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "scout_layout.h"
enum { SCOUT_MENU, SCOUT_SCAN_PAGE, SCOUT_SETTINGS, SCOUT_BENCH, SCOUT_SPECTRUM, SCOUT_CONTROL, SCOUT_REMOTE_ID, SCOUT_SIGNAL_LOG };
enum { SCOUT_NAV_NONE, SCOUT_NAV_MENU, SCOUT_NAV_SCAN, SCOUT_NAV_SETTINGS,
       SCOUT_NAV_CALIBRATE, SCOUT_NAV_LEFT, SCOUT_NAV_RIGHT, SCOUT_NAV_LED_TEST,
       SCOUT_NAV_BENCH, SCOUT_NAV_ORIENTATION, SCOUT_NAV_WALLPAPER, SCOUT_NAV_WALLPAPER_MODE,
       SCOUT_NAV_PREVIOUS, SCOUT_NAV_CHANNEL_CENTER, SCOUT_NAV_AUTO_RESCAN, SCOUT_NAV_SPECTRUM,
       SCOUT_NAV_SCAN_DISPLAY, SCOUT_NAV_CONTROL, SCOUT_NAV_REMOTE_ID, SCOUT_NAV_SIGNAL_LOG };
#ifdef SCOUT_RUST_CORE
extern int rust_scout_navigation(unsigned,int,int,bool);
#define scout_navigation rust_scout_navigation
#else
static int scout_navigation(unsigned page,int x,int y,bool landscape) {
    const int w = landscape ? CYD_H : CYD_W, h = landscape ? CYD_W : CYD_H;
    if(x<0 || y<0 || x>=w || y>=h)return SCOUT_NAV_NONE;
    bool card = x>=16 && x<w-16;
    if(page==SCOUT_MENU) {
        if(x>=SCOUT_LOG_BUTTON_X(w) && x<SCOUT_LOG_BUTTON_X(w)+SCOUT_LOG_BUTTON_W &&
           y>=SCOUT_LOG_BUTTON_Y && y<SCOUT_LOG_BUTTON_Y+SCOUT_LOG_BUTTON_H)return SCOUT_NAV_SIGNAL_LOG;
        if(!card) return SCOUT_NAV_NONE;
        if(y>=SCOUT_MENU_SCAN_Y && y<SCOUT_MENU_SCAN_Y+SCOUT_MENU_CARD_H)return SCOUT_NAV_SCAN;
        if(y>=SCOUT_MENU_CONTROL_Y && y<SCOUT_MENU_CONTROL_Y+SCOUT_MENU_CARD_H)return SCOUT_NAV_CONTROL;
        if(y>=SCOUT_MENU_SPECTRUM_Y && y<SCOUT_MENU_SPECTRUM_Y+SCOUT_MENU_CARD_H)return SCOUT_NAV_SPECTRUM;
        if(y>=SCOUT_MENU_SETTINGS_Y && y<SCOUT_MENU_SETTINGS_Y+SCOUT_MENU_CARD_H)return SCOUT_NAV_SETTINGS;
        if(y>=SCOUT_MENU_REMOTE_ID_Y && y<SCOUT_MENU_REMOTE_ID_Y+SCOUT_MENU_CARD_H)return SCOUT_NAV_REMOTE_ID;
        if(y>=SCOUT_MENU_BENCH_Y && y<SCOUT_MENU_BENCH_Y+SCOUT_MENU_CARD_H)return SCOUT_NAV_BENCH;
    } else {
        if(x>=8 && x<66 && y>=6 && y<34)return SCOUT_NAV_MENU;
        if(page==SCOUT_SCAN_PAGE && x>=SCOUT_SPEC_X && x<w-SCOUT_SPEC_X &&
           y>=SCOUT_SPEC_Y && y<SCOUT_SPEC_Y+SCOUT_SPEC_H)return SCOUT_NAV_SCAN_DISPLAY;
        if(page==SCOUT_SETTINGS && card) {
            if(y>=SCOUT_RESCAN_Y && y<SCOUT_RESCAN_Y+SCOUT_SETTING_H)return SCOUT_NAV_AUTO_RESCAN;
            if(y>=SCOUT_ORIENTATION_Y && y<SCOUT_ORIENTATION_Y+SCOUT_SETTING_H)return SCOUT_NAV_ORIENTATION;
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
