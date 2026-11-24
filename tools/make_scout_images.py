#!/usr/bin/env python3
"""Render the real C scanner UI and 4-bit font into simulated previews.

Requires a host C compiler (cc, or an activated MSVC cl environment) and Pillow.
No device, radio, or display simulation is used as detection-test evidence.
"""
from pathlib import Path
import os, shutil, subprocess, tempfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
COMP=ROOT/'espsoup/components/cyd_c5'
FAMILY=ROOT/'espsoup/main/families/c5_c6_c61'
OUT=ROOT/'docs/img'


def render():
    src=(FAMILY/'cyd_ui.h').read_text()
    state=src[src.index('typedef struct { const char *name;'):src.index('static void scout_tune(')]
    state+=src[src.index('static bool scout_tuner_valid('):src.index('static void scout_tuner_apply(')]
    state+=src[src.index('#define SCOUT_TUNER_TAB_Y'):src.index('static int scout_tuner_column(')]
    draw=src[src.index('static int scout_corner_inset('):src.index('static void cyd_ui_init(')]
    a=draw.index('static void scout_spectrum_step(');b=draw.index('static void scout_draw_scan_channel(',a)
    draw=draw[:a]+draw[b:]
    driver=(COMP/'cyd_c5.c').read_text()
    frame=driver[driver.index('bool cyd_frame_ready('):driver.index('int cyd_text_width(')]
    harness=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cyd_c5.h"
#include "cyd_wallpaper.h"
#include "fpv_scan.h"
#include "scout_version.h"
#include "scout_layout.h"
#include "scout_spectrum.h"
#include "scout_digital.h"
#include "scout_navigation.h"
#include "scout_signal_log.h"
#include "scout_remote_id.h"
#include "composite/bench.h"
#define sv_FRAME_HOLD_US 3000000
static struct { int64_t last_row_us; } sv_v;
static bool scout_log_fault;
static void scout_video_invalidate(void){}
scout_bench_view_t scout_bench;
static uint16_t canvas[CYD_W*CYD_H], band[CYD_MAX_W*CYD_BAND_LINES];
static uint16_t composed[CYD_W*CYD_H], *band_buf=band, *frame_buf;
static bool frame_open;
static int window_x,window_y,window_w,window_h,window_offset;
static int preserve_from,preserve_to,watch_x,watch_y;
static int update_top,update_bottom;
static bool protect_scan_controls;
static bool watch_control;
static uint16_t watch_color;
enum { MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2 };
static void *heap_caps_malloc(size_t size,unsigned caps) {
    assert(size==sizeof(composed) && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    return composed;
}
static bool landscape;
static bool wallpaper;
static cyd_wallpaper_mode_t wallpaper_mode;
bool cyd_landscape(void) { return landscape; }
bool cyd_wallpaper_active(void) { return wallpaper; }
bool cyd_wallpaper_update(int64_t now) { (void)now; return wallpaper; }
cyd_wallpaper_mode_t cyd_wallpaper_mode(void) { return wallpaper_mode; }
const char *cyd_wallpaper_status(void) { return "No wallpaper.png or .gif"; }
void cyd_wallpaper_row(uint16_t *pixels,int width,int height,int y) {
    assert(wallpaper && y>=0 && y<height);
    for(int x=0;x<width;x++) pixels[x]=CYD_BLUE;
}
static int16_t spec_bins[1024];
uint16_t *cyd_band(void) { return band; }
static void lcd_window(int x,int y,int w,int h) {
    assert(!frame_open && x>=0 && y>=0 && w>0 && h>0 && x+w<=cyd_width() && y+h<=cyd_height());
    assert(preserve_from==preserve_to || y+h<=preserve_from || y>=preserve_to);
    assert(!update_bottom || (y>=update_top && y+h<=update_bottom));
    if(protect_scan_controls) assert(y>=38 || x>=66);
    window_x=x;window_y=y;window_w=w;window_h=h;window_offset=0;
}
static void lcd_data(const void *data,size_t bytes) {
    const uint16_t *pixels=data;
    assert(!frame_open && bytes%2==0);
    for(size_t i=0;i<bytes/2;i++,window_offset++) {
        assert(window_offset<window_w*window_h);
        int x=window_x+window_offset%window_w,y=window_y+window_offset/window_w;
        if(watch_control && x==watch_x && y==watch_y) assert(pixels[i]==watch_color);
        canvas[y*cyd_width()+x]=pixels[i];
    }
}
'''+frame+state+draw+r'''
int main(int argc,char **argv) {
    assert(argc==2);
    assert(cyd_label_width("",CYD_FONT_UI)==0);
    assert(cyd_label_width("WWW",CYD_FONT_UI)>cyd_label_width("III",CYD_FONT_UI));
    assert(cyd_label_width("Calibrate touch",CYD_FONT_UI)<=138);
    assert(cyd_label_width("Next image",CYD_FONT_UI)<=98);
    assert(cyd_label_width("Locked LQ 100%",CYD_FONT_SMALL)<=106);
    assert(cyd_label_width("SCAN",CYD_FONT_UI)<=42);
    assert(cyd_label_width("RSSI +999dB",CYD_FONT_SMALL)<=106);
    assert(cyd_label_width("Band / Ch",CYD_FONT_UI)<102);
    assert(cyd_label_width("Tune channel",CYD_FONT_TITLE)<=CYD_W-78);
    assert(cyd_label_width("Clear",CYD_FONT_UI)<49);
    assert(cyd_label_width("~5805 MHz  /  Medium confidence",CYD_FONT_SMALL)<=CYD_W);
    assert(cyd_label_width("Control Link Scanner",CYD_FONT_UI)<=CYD_W-80);
    assert(cyd_label_width("Control Links",CYD_FONT_TITLE)<=CYD_W-86);
    assert(16+cyd_label_width("FPV Scout",CYD_FONT_TITLE)<=SCOUT_LOG_BUTTON_X(CYD_W));
    assert(cyd_label_width("Signal Log",CYD_FONT_UI)<=SCOUT_LOG_BUTTON_W-18);
    assert(78+cyd_label_width("Signal Details",CYD_FONT_TITLE)<=CYD_W);
    assert(cyd_label_width("DJI OcuSync",CYD_FONT_UI)+
           cyd_label_width("Likely 2484 MHz",CYD_FONT_SMALL)+48<=CYD_W);
    cyd_label(-4,-4,"Clipped",CYD_WHITE,CYD_BLACK,CYD_FONT_NUMBER);
    cyd_label(CYD_W-4,CYD_H-3,"Clipped",CYD_WHITE,CYD_BLACK,CYD_FONT_UI);
    cyd_label(400,400,"Offscreen",CYD_WHITE,CYD_BLACK,CYD_FONT_UI);
    const char *names[]={"scout-scanning","scout-checking","scout-video-found","scout-locked","scout-sync-lost","scout-error","scout-menu","scout-settings","scout-composite-bench","scout-spectrum","scout-waterfall","scout-tune-channel","scout-tune-frequency","scout-digital","scout-control","scout-signal-log","scout-signal-details"};
    static uint8_t composite[49920];
    static uint8_t history[SCOUT_SPEC_W*SCOUT_WATERFALL_ROWS];
    for(int i=0;i<34;i++) {
        int k=i%17;
        landscape=i>=17;
        memset(&scout,0,sizeof(scout));
        scout.auto_rescan=true;
        scout.page=k==6?SCOUT_MENU:k==7?SCOUT_SETTINGS:k==8?SCOUT_BENCH:k==9?SCOUT_SPECTRUM:k==14?SCOUT_CONTROL:SCOUT_SCAN_PAGE;
        scout.scan.mode=k==0?FPV_SCAN:k==1?FPV_VERIFY:k==2?FPV_HIT:FPV_LOCK;
        scout.scan.last_sync=k==4?0:1000000;
        scout.spectrum_valid=true;scout.floor_est=-3000;
        scout.spectrum_updated=1000000;
        scout.lq_history=0xffbf;scout.lq_samples=16;
        scout.video_live=k==2 || k==3 || k==10;scout.video_pal=true;
        for(int j=0;j<256;j++) {
            int d=j-128;if(d<0)d=-d;
            spec_bins[j]=-3000+(j*37%200)+(d<40?(40-d)*100:0);
        }
        for(int j=0;j<SCOUT_SPEC_W;j++) {
            int d=j-SCOUT_SPEC_W/2;if(d<0)d=-d;
            scout.spectrum_plot[j]=-3000+(j*37%200)+(d<28?(28-d)*140:0);
        }
        if(k==5) {scout.scan.mode=FPV_SCAN;scout.errors=3;}
        if(k==13) {
            scout.scan.mode=FPV_SCAN;
            scout.digital.shown=(scout_rf_hit_t){5795,20000,1700,landscape?SCOUT_RF_O4:SCOUT_RF_UNKNOWN};
        }
        if(k==9 || k==10 || k==14) {
            scout.waterfall=history;
            scout.waterfall_count=SCOUT_WATERFALL_ROWS;
            for(unsigned i=0;i<sizeof(history);i++) {
                int x=(int)(i%SCOUT_SPEC_W), y=(int)(i/SCOUT_SPEC_W);
                int distance=abs(x-(SCOUT_SPEC_W/2-20+y/3));
                history[i]=(uint8_t)(distance<18?180-distance*7:0);
            }
        }
        scout.scan_waterfall=k==10;
        if(k==11 || k==12) {
            scout.tuner_direct=k==12;
            scout.tuner_channel=35;
            scout.tuner_mhz=5799;
            scout_draw_tuner();
        }
        else if(scout.page==SCOUT_MENU) scout_draw_menu();
        else if(k==15 || k==16) {
            scout.page=SCOUT_SIGNAL_LOG;
            scout.log_selection=k==16?0:-1;
            scout_log_record(SCOUT_LOG_RID_BLE,0,-59,1000000);
            scout_log_record(SCOUT_LOG_ELRS,0,-32,3000000);
            scout_log_record(SCOUT_LOG_DIGITAL,5800,19,12000000);
            scout_log_record(SCOUT_LOG_ANALOG,5769,40,15000000);
            if(k==16) scout_log_lost(SCOUT_LOG_ANALOG,5769,19000000);
            scout_draw_signal_log(k==16?21000000:17000000);
        }
        else if(scout.page==SCOUT_SETTINGS) scout_draw_settings();
        else if(scout.page==SCOUT_CONTROL) {
            scout.control.elrs_hops=2;
            scout.control.elrs_sf=7;
            scout.control.elrs=(scout_control_reading_t){.dbfs=-32,.quality=91,.seen=true,.history=3};
            scout.control.passes=4;
            scout.control.dji_matches=2;
            scout.control.dji_mhz=2435;
            scout.control.dji=(scout_control_reading_t){.dbfs=-23,.quality=84,.history=3};
            scout.dirty=true;
            scout_draw_control(true);
        }
        else if(scout.page==SCOUT_SPECTRUM) scout_draw_spectrum_page(1000000);
        else if(scout.page==SCOUT_BENCH) {
            cvbs_configure(&scout_bench.frame,composite,CVBS_PAL,2);
            cvbs_sync(&scout_bench.frame);
            scout_bench.running=true;scout_bench.pattern=PAT_GRID;
            pattern_draw(&scout_bench.frame,PAT_GRID,0);
            scout.dirty=true;scout_draw_bench(1000000);
        }
        else scout_draw(1000000);
        if(scout.video_live && scout.page==SCOUT_SCAN_PAGE) {
            /* Explicitly simulated grayscale bars, not captured RF video. */
            for(int y=0;y<SCOUT_VIDEO_H;y++) {
                uint16_t *pixels=cyd_band();
                int width=SCOUT_VIDEO_W;
                for(int x=0;x<width;x++) {
                    int g=235-(x*6/width)*38;
                    pixels[x]=CYD_RGB(g,g,g);
                }
                cyd_blit(SCOUT_VIDEO_X,SCOUT_VIDEO_Y+y,SCOUT_VIDEO_W,1,pixels);
            }
        }
        if(landscape && k!=0 && k!=2 && k!=3 && k!=6 && k!=7 && k!=8 && k<9) continue;
        char path[1024];snprintf(path,sizeof(path),"%s/%s%s.ppm",argv[1],names[k],landscape?"-landscape":"");
        FILE *f=fopen(path,"wb");assert(f);
        fprintf(f,"P6\n%d %d\n255\n",cyd_width(),cyd_height());
        unsigned colors=0;uint16_t seen[256];
        for(int j=0;j<CYD_W*CYD_H;j++) {
            uint16_t c=(canvas[j]>>8)|((canvas[j]&255)<<8);
            unsigned char rgb[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
            fwrite(rgb,1,3,f);
            int found=0;for(unsigned m=0;m<colors;m++) if(seen[m]==c) found=1;
            if(!found && colors<256)seen[colors++]=c;
        }
        fclose(f);assert(colors>20); /* antialiased coverage, not a binary glyph */
    }
    wallpaper=true;
    for(int rotation=0;rotation<2;rotation++) {
        landscape=rotation!=0;
        const uint16_t bg=CYD_RGB(10,14,20);
        scout.page=SCOUT_MENU;
        wallpaper_mode=CYD_WALLPAPER_MAIN;
        scout_draw_menu();
        assert(canvas[0]==CYD_BLUE);
        assert(canvas[(SCOUT_MENU_SCAN_Y+12)*cyd_width()+cyd_width()-22]==CYD_RGB(17,24,35));
        wallpaper_mode=CYD_WALLPAPER_OFF;
        scout_draw_menu();
        assert(canvas[0]==bg);
        scout.page=SCOUT_SETTINGS;
        wallpaper_mode=CYD_WALLPAPER_MAIN;
        scout_draw_settings();
        assert(canvas[0]==bg);
        wallpaper_mode=CYD_WALLPAPER_ALL;
        scout_draw_settings();
        assert(canvas[0]==CYD_BLUE);
        watch_x=22;watch_y=100;watch_color=canvas[watch_y*cyd_width()+watch_x];
        assert(watch_color==CYD_RGB(27,38,54));
        watch_control=true;
        scout_draw_settings();
        watch_control=false;
        scout.page=SCOUT_BENCH;
        scout.dirty=true;
        scout_draw_bench(1000000);
        assert(canvas[0]==CYD_BLUE && canvas[88*cyd_width()]==CYD_WHITE);
        scout.page=SCOUT_SCAN_PAGE;
        scout.dirty=true;
        scout.video_live=false;
        scout_draw(1000000);
        assert(canvas[0]==CYD_BLUE && canvas[(SCOUT_VIDEO_Y+30)*cyd_width()]==CYD_BLUE);
        watch_x=24;watch_y=SCOUT_BUTTON_Y+16;
        watch_color=canvas[watch_y*cyd_width()+watch_x];
        assert(watch_color!=CYD_BLUE);
        watch_control=true;
        scout_draw(1200000);
        watch_control=false;
        scout.video_live=true;
        cyd_fill(0,SCOUT_VIDEO_Y,cyd_width(),SCOUT_VIDEO_H,CYD_GREEN);
        preserve_from=SCOUT_VIDEO_Y;preserve_to=SCOUT_VIDEO_Y+SCOUT_VIDEO_H;
        scout.dirty=true;
        scout_draw(1300000);
        preserve_from=preserve_to=0;
        assert(canvas[0]==CYD_BLUE && canvas[(SCOUT_VIDEO_Y+30)*cyd_width()]==CYD_GREEN);
        wallpaper_mode=CYD_WALLPAPER_OFF;
        scout.dirty=true;
        scout_draw(1400000);
        protect_scan_controls=true;
        update_bottom=SCOUT_VIDEO_Y;
        scout_draw(1600000);
        update_top=SCOUT_SPEC_Y;
        update_bottom=SCOUT_SPEC_Y+SCOUT_SPEC_H;
        scout.scan_waterfall=!scout.scan_waterfall;
        scout_draw_scan_plot();
        protect_scan_controls=false;
        update_top=update_bottom=0;
        scout.page=SCOUT_SPECTRUM;
        scout.dirty=true;
        scout_draw_spectrum_page(2000000);
        update_top=SCOUT_SPEC_Y;
        update_bottom=SCOUT_BUTTON_Y-8;
        scout_draw_spectrum_page(2200000);
        update_top=update_bottom=0;
    }
    puts("PASS: both orientations, steady controls on GIF redraw, and live plot transfers skip controls and video");
    return 0;
}
'''
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='scout-preview-') as tmp:
        temp=Path(tmp)
        (temp/'esp_err.h').write_text('typedef int esp_err_t;\n')
        (temp/'sdkconfig.h').write_text('#define CONFIG_CYD_C5_BAND_LINES 4\n')
        (temp/'preview.c').write_text(harness)
        includes=[temp,COMP/'include',FAMILY]
        composite=[str(FAMILY/'composite'/name) for name in ('cvbs.c','patterns.c')]
        cl=shutil.which('cl')
        if cl:
            command=[cl,'/nologo','/std:c11','/O2']+['/I'+str(p) for p in includes]+[str(temp/'preview.c'),str(COMP/'cyd_font_smooth.c'),*composite,'/Fe:'+str(temp/'preview.exe'),'/Fo:'+str(temp)+os.sep]
            exe=temp/'preview.exe'
        else:
            cc=shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
            if not cc: raise SystemExit('Activate a host C compiler before rendering previews')
            exe=temp/'preview'
            command=[cc,'-std=c11','-O2']+['-I'+str(p) for p in includes]+[str(temp/'preview.c'),str(COMP/'cyd_font_smooth.c'),*composite,'-o',str(exe)]
        subprocess.run(command,check=True)
        subprocess.run([str(exe),str(temp)],check=True)
        for path in temp.glob('*.ppm'):
            im=Image.open(path).convert('RGB')
            im.save(OUT/(path.stem+'.png'))


if __name__=='__main__':render()
