/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../main/families/c5_c6_c61/scout_navigation.h"
bool cyd_landscape(void) { return false; }
#define portrait(page,x,y) scout_navigation(page,x,y,false)
#define landscape(page,x,y) scout_navigation(page,x,y,true)
int main(void) {
    assert(portrait(SCOUT_MENU,120,60)==SCOUT_NAV_HOME_ANALOG);
    assert(portrait(SCOUT_MENU,120,105)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_MENU,120,120)==SCOUT_NAV_HOME_DIGITAL);
    assert(portrait(SCOUT_MENU,120,165)==SCOUT_NAV_CONTROL);
    assert(portrait(SCOUT_MENU,180,270)==SCOUT_NAV_SETTINGS);
    assert(portrait(SCOUT_SCAN_PAGE,30,20)==SCOUT_NAV_MENU);
    assert(portrait(SCOUT_SETTINGS,30,20)==SCOUT_NAV_MENU);
    assert(portrait(SCOUT_SETTINGS,120,44)==SCOUT_NAV_AUTO_RESCAN);
    assert(portrait(SCOUT_SETTINGS,120,75)==SCOUT_NAV_AUTO_RESCAN);
    assert(portrait(SCOUT_SETTINGS,120,76)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,120,80)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,120,96)==SCOUT_NAV_ORIENTATION);
    assert(portrait(SCOUT_SETTINGS,98,185)==SCOUT_NAV_WALLPAPER_MODE);
    assert(portrait(SCOUT_SETTINGS,120,185)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,190,185)==SCOUT_NAV_WALLPAPER);
    assert(portrait(SCOUT_SETTINGS,120,220)==SCOUT_NAV_CALIBRATE);
    assert(portrait(SCOUT_SETTINGS,120,250)==SCOUT_NAV_LED_TEST);
    assert(portrait(SCOUT_SETTINGS,120,229)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,120,288)==SCOUT_NAV_START_PAGE);
    assert(portrait(SCOUT_SETTINGS,120,306)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SCAN_PAGE,50,SCOUT_BUTTON_Y+10)==SCOUT_NAV_LEFT);
    assert(portrait(SCOUT_SCAN_PAGE,180,SCOUT_BUTTON_Y+10)==SCOUT_NAV_CHANNEL_CENTER);
    assert(portrait(SCOUT_SCAN_PAGE,140,SCOUT_BUTTON_Y+10)==SCOUT_NAV_PREVIOUS);
    assert(portrait(SCOUT_SCAN_PAGE,216,SCOUT_BUTTON_Y+10)==SCOUT_NAV_RIGHT);
    assert(portrait(SCOUT_SCAN_PAGE,120,180)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,120,117)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,120,128)==SCOUT_NAV_SIGNAL_CAL);
    assert(portrait(SCOUT_SETTINGS,120,150)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_MENU,60,270)==SCOUT_NAV_SPECTRUM);
    assert(portrait(SCOUT_MENU,180,302)==SCOUT_NAV_BENCH);
    assert(portrait(SCOUT_MENU,120,298)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_MENU,120,240)==SCOUT_NAV_REMOTE_ID);
    assert(portrait(SCOUT_MENU,60,298)==SCOUT_NAV_SIGNAL_LOG);
    assert(portrait(SCOUT_MENU,228,20)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SIGNAL_LOG,30,20)==SCOUT_NAV_MENU);
    assert(portrait(SCOUT_SIGNAL_LOG,180,300)==SCOUT_NAV_RIGHT);
    assert(portrait(SCOUT_REMOTE_ID,30,20)==SCOUT_NAV_MENU);
    assert(portrait(SCOUT_BENCH,30,20)==SCOUT_NAV_MENU);
    assert(portrait(SCOUT_BENCH,50,SCOUT_BUTTON_Y+10)==SCOUT_NAV_LEFT);
    assert(portrait(SCOUT_BENCH,180,SCOUT_BUTTON_Y+10)==SCOUT_NAV_RIGHT);
    assert(portrait(SCOUT_MENU,-1,120)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SCAN_PAGE,11,38)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SCAN_PAGE,30,38)==SCOUT_NAV_SCAN_DISPLAY);
    assert(portrait(SCOUT_SCAN_PAGE,227,64)==SCOUT_NAV_SCAN_DISPLAY);
    assert(portrait(SCOUT_SCAN_PAGE,30,65)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SCAN_PAGE,120,90)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SCAN_PAGE,120,SCOUT_BUTTON_Y+10)==SCOUT_NAV_NONE);
    assert(portrait(SCOUT_SETTINGS,CYD_W,320)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_MENU,280,65)==SCOUT_NAV_HOME_ANALOG);
    assert(landscape(SCOUT_MENU,280,96)==SCOUT_NAV_HOME_DIGITAL);
    assert(landscape(SCOUT_MENU,280,129)==SCOUT_NAV_CONTROL);
    assert(landscape(SCOUT_CONTROL,30,20)==SCOUT_NAV_MENU);
    assert(landscape(SCOUT_MENU,100,198)==SCOUT_NAV_SPECTRUM);
    assert(landscape(SCOUT_SPECTRUM,280,220)==SCOUT_NAV_RIGHT);
    assert(portrait(SCOUT_SPECTRUM,30,20)==SCOUT_NAV_MENU);
    assert(landscape(SCOUT_MENU,280,198)==SCOUT_NAV_SETTINGS);
    assert(landscape(SCOUT_MENU,280,176)==SCOUT_NAV_REMOTE_ID);
    assert(landscape(SCOUT_MENU,280,224)==SCOUT_NAV_BENCH);
    assert(landscape(SCOUT_MENU,100,224)==SCOUT_NAV_SIGNAL_LOG);
    assert(landscape(SCOUT_SIGNAL_LOG,80,220)==SCOUT_NAV_LEFT);
    assert(landscape(SCOUT_SETTINGS,100,57)==SCOUT_NAV_AUTO_RESCAN);
    assert(landscape(SCOUT_SETTINGS,160,57)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_SETTINGS,280,57)==SCOUT_NAV_START_PAGE);
    assert(landscape(SCOUT_SETTINGS,305,57)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_SETTINGS,280,96)==SCOUT_NAV_ORIENTATION);
    assert(landscape(SCOUT_SETTINGS,160,128)==SCOUT_NAV_SIGNAL_CAL);
    assert(landscape(SCOUT_SETTINGS,100,212)==SCOUT_NAV_CALIBRATE);
    assert(landscape(SCOUT_SETTINGS,160,212)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_SETTINGS,280,212)==SCOUT_NAV_LED_TEST);
    assert(landscape(SCOUT_SETTINGS,280,164)==SCOUT_NAV_WALLPAPER);
    assert(landscape(SCOUT_SETTINGS,100,164)==SCOUT_NAV_WALLPAPER_MODE);
    assert(landscape(SCOUT_SETTINGS,160,164)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_BENCH,30,20)==SCOUT_NAV_MENU);
    assert(landscape(SCOUT_BENCH,140,220)==SCOUT_NAV_LEFT);
    assert(landscape(SCOUT_SCAN_PAGE,280,220)==SCOUT_NAV_RIGHT);
    assert(landscape(SCOUT_SCAN_PAGE,280,64)==SCOUT_NAV_SCAN_DISPLAY);
    assert(landscape(SCOUT_SCAN_PAGE,280,65)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_SCAN_PAGE,160,220)==SCOUT_NAV_NONE);
    assert(landscape(SCOUT_SCAN_PAGE,280,240)==SCOUT_NAV_NONE);
    puts("PASS: Portrait/Landscape selector, main menu, Back, calibration, scan and Composite Bench");
}
