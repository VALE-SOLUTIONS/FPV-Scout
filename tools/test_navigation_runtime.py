#!/usr/bin/env python3
"""Exercise actual navigation/input/UI-step code with counted RF stubs."""
from pathlib import Path
import os,shutil,subprocess,tempfile
R=Path(__file__).resolve().parents[1];F=R/'espsoup/main/families/c5_c6_c61'
s=(F/'cyd_ui.h').read_text()
state=s[s.index('typedef struct { const char *name;'):s.index('static void scout_tune(')]
tune=s[s.index('static void scout_tune('):s.index('static int32_t scout_measure_signal(')]
nav=s[s.index('static void scout_clear_signal('):s.index('static int scout_corner_inset(')]
init=s[s.index('static void cyd_ui_init('):s.index('static void cyd_ui_host_command(')]
step=s[s.index('static bool scout_read_evidence('):]
command=s[s.index('static bool cyd_ui_command('):s.index('static bool scout_read_evidence(')]
spectrum_history=s[s.index('static bool scout_update_spectrum('):s.index('static void scout_draw_scan_channel(')]
code='''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scout_navigation.h"
#include "cyd_wallpaper.h"
#include "fpv_scan.h"
#include "scout_spectrum.h"
#include "scout_digital.h"
#include "scout_signal_log.h"
#include "scout_indicator.h"
#include "scout_version.h"
#include "composite/bench.h"
static int64_t clock_us=1000000;
static int64_t esp_timer_get_time(void){return clock_us;}
static unsigned captures,spectrum_calls,calibrations,orientation_loads,menu_draws,settings_draws,led_tests,led_hz;
static unsigned opens,closes,bench_draws,bench_steps;
static unsigned rid_starts,rid_stops,rid_steps;
static unsigned rid_test_wifi,rid_test_ble;
static struct {unsigned channel,matches,ble_matches,latest_wifi_channel;int latest_wifi_rssi,latest_ble_rssi;bool ble_enabled,ble_window;} rid_state,*scout_rid;
#define SCOUT_RID_EXPIRY_US 30000000LL
#define sv_FRAME_HOLD_US 3000000LL
static bool log_sd_ok;
static void scout_log_load(void){}
static void scout_log_checkpoint(int64_t now){(void)now;}
static bool scout_log_clear(void){scout_log.count=0;return true;}
static void scout_log_save_to_sd(char *status,size_t size){
    if(log_sd_ok)scout_log_clear();
    snprintf(status,size,"%s",log_sd_ok?"Saved SC000001.CSV":"Save failed. Log kept.");
}
static void scout_rid_start(void){scout_rid=&rid_state;rid_starts++;}
static void scout_rid_stop(void){scout_rid=NULL;rid_stops++;}
static void scout_rid_poll(int64_t now){
    (void)now;rid_steps++;
    rid_state.matches+=rid_test_wifi+rid_test_ble;
    rid_state.ble_matches+=rid_test_ble;
    rid_test_wifi=rid_test_ble=0;
}
static unsigned scout_rid_frames(void){return rid_steps;}
static unsigned scout_rid_ble_frames(void){return 0;}
static bool stop_allowed=true;
scout_bench_view_t scout_bench;
const char *const pattern_names[PAT_COUNT]={"black","bars","grid","ramp","levels","bounce"};
void scout_bench_open(void){opens++;scout_bench=(scout_bench_view_t){.running=true};}
bool scout_bench_close(void){closes++;if(!stop_allowed)return false;scout_bench.running=false;return true;}
void scout_bench_select(pattern_t p){scout_bench.pattern=p;}
void scout_bench_next(void){scout_bench.pattern=(scout_bench.pattern+1)%PAT_COUNT;}
void scout_bench_format(cvbs_mode_t m){scout_bench.frame.mode=m;}
bool scout_bench_levels(unsigned lo,unsigned hi){return lo==1 && hi<=3 && hi>lo;}
void scout_bench_step(int64_t now){(void)now;bench_steps++;}
static char response[320];
static void reply(const char *line){snprintf(response,sizeof(response),"%s",line);}
enum { MALLOC_CAP_INTERNAL=1, MALLOC_CAP_8BIT=2, MALLOC_CAP_SPIRAM=4 };
static unsigned heap_caps_get_free_size(unsigned caps){(void)caps;return 0;}
static bool capture_fail=true;
void cyd_indicator_rate(unsigned hz){led_hz=hz;}
void cyd_indicator_test(void){led_tests++;}
static bool pressed; static int touch_x,touch_y;
static bool landscape;
static unsigned wallpaper_ticks, wallpaper_reloads, wallpaper_choices;
static cyd_wallpaper_mode_t wallpaper_mode;
bool cyd_landscape(void){return landscape;}
void cyd_set_landscape(bool value){landscape=value;}
bool cyd_wallpaper_update(int64_t now){(void)now;wallpaper_ticks++;return false;}
bool cyd_wallpaper_active(void){return true;}
bool cyd_frame_ready(void){return true;}
cyd_wallpaper_mode_t cyd_wallpaper_mode(void){return wallpaper_mode;}
void cyd_wallpaper_next_mode(void){wallpaper_mode=(wallpaper_mode+1)%3;}
void cyd_wallpaper_reload(void){wallpaper_reloads++;}
void cyd_wallpaper_next_file(void){wallpaper_choices++;}
const char *cyd_wallpaper_status(void){return "PNG loaded 240 x 320";}
bool cyd_touch(int *x,int *y){*x=touch_x;*y=touch_y;return pressed;}
bool cyd_boot_pressed(void){return false;}
void cyd_touch_calibrate(bool force){if(force)calibrations++;else orientation_loads++;}
void cyd_reclaim_pins(void){}
enum { ESP_OK=0, NVS_READONLY, NVS_READWRITE };
typedef int nvs_handle_t;
static int saved_rescan=-1;
static unsigned commits;
static esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle) {
    assert(strcmp(name,"cyd")==0);*handle=mode;return ESP_OK;
}
static esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *value) {
    assert(handle==NVS_READONLY && strcmp(key,"auto_rescan")==0);
    if(saved_rescan<0)return -1;
    *value=(uint8_t)saved_rescan;return ESP_OK;
}
static esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value) {
    assert(handle==NVS_READWRITE && strcmp(key,"auto_rescan")==0);
    saved_rescan=value;return ESP_OK;
}
static esp_err_t nvs_commit(nvs_handle_t handle) {assert(handle==NVS_READWRITE);commits++;return ESP_OK;}
static void nvs_close(nvs_handle_t handle){(void)handle;}
void cyd_wallpaper_init(void){}
esp_err_t cyd_init_with_bus_hook(void (*setup)(void)){setup();return ESP_OK;}
esp_err_t cyd_indicator_init(void){return ESP_OK;}
int cyd_label_width(const char *text,cyd_font_t font){return 0;}
void cyd_label(int x,int y,const char *text,uint16_t fg,uint16_t bg,cyd_font_t font){}
void cyd_fill(int x,int y,int w,int h,uint16_t colour){}
static void *heap_caps_malloc(size_t size,unsigned caps){
    static int16_t bins[FPV_BINS];
    if(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))return malloc(size);
    assert(size==sizeof(bins) && caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    return bins;
}
static void scout_video_init(void){}
static struct {bool pal, lv_ok, tv_ok;unsigned rows;int64_t last_sync_us,last_row_us;} sv_v;
static void scout_video_reset(void){memset(&sv_v,0,sizeof(sv_v));}
static void scout_video_set_polarity(int polarity){(void)polarity;}
static void scout_video_render(int64_t now){(void)now;}
static unsigned fullscreen_renders, video_invalidations;
static void scout_video_render_mode(int64_t now,const char *overlay){(void)now;assert(strstr(overlay,"RSSI"));fullscreen_renders++;}
static void scout_video_invalidate(void){video_invalidations++;}
static uint32_t raw[FPV_SAMPLES];
#define DUMP_BUF raw
'''+state+'''
static unsigned frequency_mhz,rx_channel_mode;
static int c5_khz_offset,rx_analog_filter;
static bool rx_ready;
static unsigned rx_bandwidth_phy_mode(unsigned width){return width;}
static int rx_bandwidth_dcap(unsigned width){return (int)width;}
static void prepare_rx(void){rx_ready=true;}
'''+tune+'''
static bool scripted;
static unsigned samples_seen[SCOUT_COUNT];
static int32_t strengths[SCOUT_COUNT];
static int32_t scout_measure_signal(void){return strengths[scout.channel];}
static fpv_evidence_t scripted_detect(const uint32_t *w,int16_t *bins) {
    if (!scripted) return fpv_detect(w,bins);
    unsigned ch=scout.channel, sample=++samples_seen[ch];
    if (!strengths[ch]) return (fpv_evidence_t){0};
    if (!scout.comparing) return (fpv_evidence_t){7,0,1};
    if (ch==22) return (fpv_evidence_t){10,0,1}; /* strong H only */
    if (sample%4==0) return (fpv_evidence_t){0,3,1};
    return (fpv_evidence_t){ch==20?10:5,0,1};
}
#undef fpv_detect
#define fpv_detect scripted_detect
static const char *fast_capture(unsigned n,unsigned rate,uint32_t *us){assert(n==FPV_SAMPLES && rate==2);*us=0;captures++;return capture_fail?"test_failure":NULL;}
static bool video_rf, video_rows, video_sync;
static unsigned scout_video_quality(int polarity){(void)polarity;return 0;}
static bool scout_video_capture(void){
    captures++;
    if(video_rows){
        sv_v.last_row_us=clock_us;
        sv_v.tv_ok=true;
    }
    if(video_sync)sv_v.last_sync_us=clock_us;
    return video_rf;
}
static bool scout_video_live(int64_t now){(void)now;return false;}
static void scout_spectrum_step(int64_t now){
    scout.spectrum_tick=now;spectrum_calls++;
    if(scout.page==SCOUT_SPECTRUM || scout.page==SCOUT_CONTROL){
        scout.spectrum_updated=now;scout.spectrum_valid=true;
        scout.spectrum_plot[0]=scout.floor_est+2500;
    }
}
static void scout_draw_spectrum_page(int64_t now){scout.dirty=false;scout.last_draw=now;}
static unsigned activity_draws, active_channel;
static void scout_draw_scan_channel(bool sweeping){
    assert(sweeping);activity_draws++;active_channel=scout.channel;
}
static void scout_draw_scan_plot(void){scout.scan_plot_dirty=false;}
static void scout_draw(int64_t now){scout.dirty=false;scout.last_draw=now;scout_draw_scan_plot();}
static void scout_draw_menu(void){scout.dirty=false;menu_draws++;}
static void scout_draw_signal_log(int64_t now){scout.dirty=false;scout.last_draw=now;}
static void scout_draw_remote_id(int64_t now){scout.dirty=false;scout.last_draw=now;}
static void scout_draw_control(bool waterfall_changed){(void)waterfall_changed;scout.dirty=false;}
static void scout_draw_tuner(void){scout.dirty=false;}
static void scout_draw_settings(void){scout.dirty=false;settings_draws++;}
static void scout_draw_bench(int64_t now){scout.dirty=false;scout.last_draw=now;bench_draws++;}
'''+nav+init+command+spectrum_history+step+'''
static void tap(int x,int y){pressed=false;scout_input();touch_x=x;touch_y=y;pressed=true;scout_input();pressed=false;scout_input();}
static void test_signal_log(void) {
    memset(&scout_log,0,sizeof(scout_log));
    scout_log.run=1;
    landscape=false;
    scout_open_page(SCOUT_MENU);
    tap(180,20);
    assert(scout.page==SCOUT_SIGNAL_LOG && scout.log_page==0);
    unsigned before=captures;
    cyd_ui_step();
    assert(captures==before && cyd_ui_command("SCOUT?") && strstr(response,"mode=LOG entries=0"));
    tap(30,20);tap(120,92);
    scout.manual_mhz=5769;
    scout_confirmed();
    assert(scout_log.count==1 && scout_log.entries[0].mhz==5769 && scout_log.entries[0].kind==SCOUT_LOG_ANALOG);
    scout_log_record(SCOUT_LOG_DIGITAL,5800,26,clock_us);
    scout_log_record(SCOUT_LOG_O4,5802,28,clock_us+1000000);
    assert(scout_log.count==2 && scout_log.entries[0].kind==SCOUT_LOG_O4);
    scout_log_record(SCOUT_LOG_ANALOG,5769,17,clock_us+2000000);
    assert(scout_log.count==2 && scout_log.entries[0].mhz==5769 && scout_log.entries[1].mhz==5802);
    assert(scout_log.entries[0].seen_seconds==(uint32_t)(clock_us/1000000)+2);
    scout_log_lost(SCOUT_LOG_ANALOG,5769,clock_us+5000000);
    assert(scout_log.entries[0].first_seconds==(uint32_t)(clock_us/1000000));
    assert(scout_log.entries[0].strength==17 && scout_log.entries[0].lost_seconds==(uint32_t)(clock_us/1000000)+5);
    scout_open_page(SCOUT_CONTROL);
    scout.control.elrs_hops=2;scout.control.elrs_at=clock_us;
    scout.control.dji_matches=2;scout.control.dji_at=clock_us;scout.control.dji_mhz=2435;
    scout_control_step(clock_us);
    assert(scout_log.count==4 && scout_log.entries[0].kind==SCOUT_LOG_DJI_CONTROL);
    assert(scout_log.entries[1].kind==SCOUT_LOG_ELRS && scout_log.entries[1].mhz==0);
    scout_open_page(SCOUT_REMOTE_ID);
    rid_test_wifi=rid_test_ble=1;
    cyd_ui_step();
    assert(scout_log.count==6 && scout_log.entries[0].kind==SCOUT_LOG_RID_WIFI);
    assert(scout_log.entries[1].kind==SCOUT_LOG_RID_BLE && scout_log.entries[1].mhz==0);
    cyd_ui_step();assert(scout_log.count==6);
    scout_log.count=0;
    for(unsigned i=0;i<35;i++) scout_log_record(SCOUT_LOG_ANALOG,5600+i,0,clock_us);
    assert(scout_log.count==SCOUT_LOG_CAPACITY && scout_log.entries[0].mhz==5631);
    assert(scout_log.entries[SCOUT_LOG_CAPACITY-1].mhz==5600);
    tap(30,20);tap(180,20);
    assert(scout.page==SCOUT_SIGNAL_LOG && scout.log_page==0);
    for(unsigned i=0;i<12;i++) tap(180,48);
    assert(scout.log_page==(SCOUT_LOG_CAPACITY-1)/SCOUT_LOG_ROWS(320));
    tap(60,48);assert(scout.log_page==(SCOUT_LOG_CAPACITY-1)/SCOUT_LOG_ROWS(320)-1);
    tap(60,78);assert(scout.log_selection>=0);
    tap(30,20);assert(scout.log_selection<0 && scout.page==SCOUT_SIGNAL_LOG);
    tap(60,300);assert(strstr(scout.log_status,"Save failed") && scout_log.count==SCOUT_LOG_CAPACITY);
    tap(30,20);assert(scout.page==SCOUT_MENU && scout_log.count==SCOUT_LOG_CAPACITY);
    landscape=true;
    tap(254,20);assert(scout.page==SCOUT_SIGNAL_LOG && scout.log_page==0);
    tap(250,48);assert(scout.log_page==1);
    tap(70,48);assert(scout.log_page==0);
    log_sd_ok=true;
    tap(80,220);assert(!scout_log.count && strstr(scout.log_status,"Saved"));
    scout_log_record(SCOUT_LOG_ANALOG,5769,0,clock_us);
    tap(250,220);assert(scout.log_clear_confirm && scout_log.count==1);
    tap(80,220);assert(!scout.log_clear_confirm && scout_log.count==1);
    tap(250,220);tap(250,220);assert(!scout_log.count);
}
static void test_fullscreen_video(void) {
    scout_open_page(SCOUT_SCAN_PAGE);
    scout.manual=true;
    scout.video_live=true;
    scout_confirmed();
    unsigned prev=video_invalidations;
    tap(150,SCOUT_VIDEO_Y+20);
    assert(scout.video_full && video_invalidations==prev+1);
    scout_scan_refresh(clock_us);
    assert(fullscreen_renders==1);
    tap(30,20);
    assert(!scout.video_full && scout.page==SCOUT_SCAN_PAGE && video_invalidations==prev+2);
}
static void test_overlapping_channels(void) {
    scout_open_page(SCOUT_SCAN_PAGE);
    scout.candidate_count=3;
    scout.candidates[0]=(scout_candidate_t){.channel=18,.sync={.mode=FPV_HIT,.polarity=1},
        .observations=10,.h_pairs=90,.video_checks=6,.clean_lines=24}; /* B3 */
    scout.candidates[1]=(scout_candidate_t){.channel=3,.sync={.mode=FPV_HIT,.polarity=1},
        .observations=10,.h_pairs=65,.video_checks=6,.clean_lines=48}; /* R4 */
    scout.candidates[2]=(scout_candidate_t){.channel=39,.sync={.mode=FPV_HIT,.polarity=1},
        .observations=10,.h_pairs=80,.video_checks=6,.clean_lines=60}; /* F8 */
    scout_finish_comparison(clock_us);
    assert(scout.channel==3 && scout.scan.mode==FPV_LOCK);
    scout.candidate_count=3;
    scout.candidates[1].clean_lines=29;
    scout_finish_comparison(clock_us);
    assert(scout.channel==18 && scout.scan.mode==FPV_LOCK);
    scout.candidate_count=3;
    scout.candidates[1].video_checks=3;
    scout_finish_comparison(clock_us);
    assert(scout.channel==18 && scout.scan.mode==FPV_LOCK);
}
static void test_tuner(bool rotated) {
    landscape=rotated;
    scout_open_page(SCOUT_SCAN_PAGE);
    scout.manual=true;
    scout.channel=0;
    const int w=cyd_width(), apply=cyd_height()-20;
    const int row=rotated?113:140, pitch=rotated?34:48;
    const int x0=30,x1=12+(w-18)*3/8-3,x2=12+(w-18)*5/8-3,x3=w-25;
    tap(w*3/4,apply);
    assert(scout.tuner_open && !scout.tuner_direct && scout.channel==0);
    unsigned before=captures;
    cyd_ui_step();
    assert(captures==before && cyd_ui_command("SCOUT?") && strstr(response,"mode=TUNE"));
    tap(12+(w-18)*3/4-3,row); /* F band */
    tap(x3,row+pitch); /* F4 */
    tap(w/2,apply);
    assert(!scout.tuner_open && scout.channel==35 && !scout.manual_mhz);
    cyd_ui_step();assert(frequency_mhz==5800);
    tap(w*3/4,apply);tap(x3,57); /* Frequency */
    tap(x1,row+pitch); /* 5 */
    tap(x0,row+2*pitch); /* 7 */
    tap(w/2,apply); /* incomplete */
    assert(scout.tuner_open && scout.tuner_mhz==57 && scout_current_channel().mhz==5800);
    tap(x2,row+2*pitch);tap(x2,row+2*pitch); /* 99 */
    tap(x3,row+pitch); /* Delete */
    assert(scout.tuner_mhz==579);
    tap(x2,row+2*pitch);
    tap(w/2,apply);
    assert(!scout.tuner_open && scout.manual && scout.manual_mhz==5799);
    cyd_ui_step();assert(frequency_mhz==5799 && cyd_ui_command("SCOUT?") && strstr(response,"channel=Custom"));
    tap(w*3/4,apply);
    assert(scout.tuner_open && scout.tuner_direct);
    tap(x3,row); /* Clear */
    tap(x2,row+2*pitch);tap(x2,row+2*pitch);tap(x2,row+2*pitch);tap(x2,row+2*pitch);
    tap(w/2,apply);assert(scout.tuner_open && scout.tuner_mhz==9999);
    tap(33,20);
    assert(!scout.tuner_open && scout.page==SCOUT_SCAN_PAGE && scout.manual_mhz==5799);
    tap(w/2+17,apply);
    assert(scout_current_channel().mhz==5790 && !scout.manual_mhz);
    tap(w*3/4,apply);tap(x3,57);
    tap(x1,row+pitch);tap(x1,row+2*pitch);tap(x3,row+2*pitch);tap(x3,row+2*pitch);
    tap(w/2,apply); /* 5800 resolves to F4 */
    assert(!scout.tuner_open && scout.channel==35 && !scout.manual_mhz);
    tap(w/4,apply);assert(!scout.manual && !scout.manual_mhz && scout.channel==0);
}
int main(void){
    cyd_ui_init();
    assert(scout.up && scout.page==SCOUT_SCAN_PAGE && !scout.manual);
    assert(orientation_loads==1 && !scout.sweep_start && scout.auto_rescan);
    cyd_ui_step();
    assert(captures==1 && scout.scan.mode==FPV_SCAN);
    tap(30,20);assert(scout.page==SCOUT_MENU);
    captures=0;spectrum_calls=0;orientation_loads=0;
    scout.scan.mode=FPV_LOCK;scout.video_live=true;
    cyd_ui_step();cyd_ui_step();
    assert(menu_draws==1 && captures==0 && spectrum_calls==0 && wallpaper_ticks==2);
    tap(120,210);assert(scout.page==SCOUT_SETTINGS);
    cyd_ui_step();assert(settings_draws==1 && captures==0 && spectrum_calls==0 && wallpaper_ticks==2);
    tap(120,96);assert(landscape && orientation_loads==1 && scout.page==SCOUT_SETTINGS);
    tap(280,164);assert(wallpaper_choices==1 && wallpaper_reloads==0);
    tap(280,212);assert(led_tests==1);
    tap(100,212);assert(calibrations==1);
    tap(30,20);assert(scout.page==SCOUT_MENU);
    tap(280,65);assert(scout.page==SCOUT_SCAN_PAGE);
    tap(30,20);tap(280,145);assert(scout.page==SCOUT_SETTINGS);
    tap(280,96);assert(!landscape && orientation_loads==2 && scout.page==SCOUT_SETTINGS);
    assert(captures==0 && spectrum_calls==0);
    tap(90,184);assert(wallpaper_mode==CYD_WALLPAPER_ALL && scout.dirty);
    assert(scout_wallpaper_visible());
    tap(90,184);assert(wallpaper_mode==CYD_WALLPAPER_OFF && !scout_wallpaper_visible());
    tap(90,184);assert(wallpaper_mode==CYD_WALLPAPER_MAIN && !scout_wallpaper_visible());
    tap(180,184);assert(wallpaper_choices==2 && scout.page==SCOUT_SETTINGS);
    assert(cyd_ui_command("WALLPAPER?") && strstr(response,"PNG loaded") && wallpaper_reloads==0);
    assert(cyd_ui_command("WALLPAPER RELOAD") && strstr(response,"PNG loaded") && wallpaper_reloads==1);
    tap(120,250);assert(led_tests==2 && captures==0);
    touch_x=120;touch_y=220;pressed=true;
    scout_input();scout_input();assert(calibrations==2);
    pressed=false;scout_input();
    tap(30,20);assert(scout.page==SCOUT_MENU);
    scout.scan=(fpv_scan_t){.mode=FPV_VERIFY,.h_bursts=3,.v_bursts=1,.last_sync=100};
    tap(120,133);assert(scout.page==SCOUT_CONTROL && scout.waterfall);
    cyd_ui_step();assert(scout.page==SCOUT_CONTROL && scout.waterfall_count==1);
    assert(cyd_ui_command("SCOUT?") && strstr(response,"mode=CONTROL") && strstr(response,"waterfall_rows=1"));
    assert(scout_control_probability(&(scout_control_reading_t){.seen=true},0)==67);
    assert(scout_control_probability(&(scout_control_reading_t){.history=0xFF,.seen=true},9)==90);
    assert(scout_control_probability(&(scout_control_reading_t){.history=0x07},8)==40);
    tap(30,20);assert(!scout.waterfall);
    tap(120,92);assert(scout.page==SCOUT_SCAN_PAGE);
    assert(cyd_ui_command("WALLPAPER RELOAD") && strstr(response,"open_menu_or_settings") && wallpaper_reloads==1);
    assert(scout.scan.mode==FPV_SCAN && !scout.scan.h_bursts && !scout.scan.v_bursts && !scout.scan.last_sync && !scout.video_live && !scout.tuned);
    assert(scout.waterfall && !scout.scan_waterfall);
    tap(120,60);assert(scout.scan_waterfall && scout.scan_plot_dirty && scout.scan.mode==FPV_SCAN);
    tap(120,110);assert(scout.scan_waterfall);
    tap(120,60);assert(!scout.scan_waterfall && scout.scan.mode==FPV_SCAN);
    cyd_ui_step();assert(captures==1 && wallpaper_ticks==2);
    tap(30,20);assert(scout.page==SCOUT_MENU);
    unsigned before=captures;clock_us+=10000000;
    cyd_ui_step();assert(captures==before);
    tap(120,92);capture_fail=false;scout.errors=0;
    static int16_t bins[FPV_BINS];scout.bins=bins;
    scout.scan.mode=FPV_LOCK;scout.scan.last_sync=clock_us;
    scout.spectrum_valid=true;scout.spectrum_updated=clock_us;scout.spectrum_tick=clock_us;
    scout.floor_est=0;for(unsigned x=0;x<SCOUT_SPEC_W;x++)scout.spectrum_plot[x]=3000;
    cyd_ui_step();assert(led_hz==30);
    assert(scout_rssi(clock_us)==3000 && scout_lq()==0);
    scout.lq_history=0xff00;scout.lq_samples=16;
    assert(scout_lq()==50 && cyd_ui_command("SCOUT?") && strstr(response,"rssi=+30dB lq=50%"));
    clock_us+=1000001;cyd_ui_step();assert(led_hz==0);
    scout.scan.last_sync=clock_us;scout.spectrum_updated=clock_us-5000001;
    cyd_ui_step();assert(led_hz==0 && scout_rssi(clock_us)==INT32_MIN);
    clock_us+=100000;scout.spectrum_updated=clock_us;capture_fail=true;
    cyd_ui_step();assert(led_hz==0);
    led_hz=30;tap(30,20);assert(led_hz==0);
    assert(cyd_ui_command("BENCH black") && strstr(response,"open_composite_bench"));
    assert(!opens && !scout_bench.running);
    before=captures;unsigned prev_sweep=spectrum_calls;
    tap(120,273);
    assert(scout.page==SCOUT_BENCH && opens==1 && scout_bench.running && scout_bench.pattern==PAT_BLACK);
    cyd_ui_step();assert(bench_draws==1 && bench_steps==1 && captures==before && spectrum_calls==prev_sweep);
    tap(170,300);assert(scout_bench.pattern==PAT_BARS);
    tap(60,300);assert(scout_bench.frame.mode==CVBS_PAL);
    assert(cyd_ui_command("CAP 512 1") && strstr(response,"wired_bench_active"));
    assert(cyd_ui_command("BENCH ramp") && scout_bench.pattern==PAT_RAMP);
    stop_allowed=false;tap(30,20);assert(scout.page==SCOUT_BENCH && scout_bench.running);
    stop_allowed=true;tap(30,20);assert(scout.page==SCOUT_MENU && closes==2 && !scout_bench.running);
    tap(120,92);cyd_ui_step();assert(captures==before+1);
    unsigned channel=scout.channel;
    tap(50,300);assert(scout.manual && !scout.next_check);
    before=captures;
    cyd_ui_step();cyd_ui_step();
    assert(scout.channel==channel && captures==before+2); /* no auto pacing after capture error */
    capture_fail=false;
    scout.scan=(fpv_scan_t){.mode=FPV_VERIFY,.deadline=clock_us-1};
    cyd_ui_step();assert(scout.scan.mode==FPV_SCAN && scout.channel==channel);
    tap(180,300);assert(scout.tuner_open && scout.channel==channel);
    tap(30,20);assert(!scout.tuner_open && scout.page==SCOUT_SCAN_PAGE);
    tap(216,300);assert(scout.channel==(channel+1)%SCOUT_COUNT);
    tap(140,300);assert(scout.channel==channel);
    scout.channel=SCOUT_COUNT-1;
    unsigned sweeps=scout.sweeps;
    scout.scan.mode=FPV_HIT;
    scout_confirmed();assert(scout.manual && scout.scan.mode==FPV_LOCK);
    tap(216,300);assert(scout.manual && scout.channel==0 && scout.sweeps==sweeps && scout.scan.mode==FPV_SCAN);
    assert(cyd_ui_command("SCOUT?") && strstr(response,"control=MANUAL"));
    tap(50,300);assert(!scout.manual);
    tap(120,300);assert(scout.manual); /* Full-width Manual, including the paired-button gap. */
    tap(50,300);assert(!scout.manual && scout.channel==0);
    activity_draws=0;
    scripted=true;
    strengths[18]=100;strengths[20]=300;strengths[22]=400;strengths[39]=500;
    before=captures;
    for (unsigned i=0;i<SCOUT_COUNT;i++) {
        assert(scout.channel==i && scout.scan.mode==FPV_SCAN && !scout.comparing);
        clock_us+=3000;
        cyd_ui_step();
    }
    assert(captures==before+SCOUT_COUNT && scout.comparing && scout.sweep_us==47*3000);
    assert(activity_draws>1 && active_channel>0 && active_channel<SCOUT_COUNT);
    assert(scout.candidate_count==3 && scout.candidates[0].channel==39 &&
           scout.candidates[1].channel==22 && scout.candidates[2].channel==20);
    assert(cyd_ui_command("SCOUT?") && strstr(response,"mode=COMPARE"));
    for (unsigned i=0;scout.comparing && i<120;i++) {
        clock_us+=3000;
        cyd_ui_step();
    }
    assert(scout.channel==20 && scout.scan.mode==FPV_LOCK && scout.last_compared==3 && scout.lock_us<1000000);
    tap(120,60);assert(scout.scan_waterfall && scout.channel==20 && scout.scan.mode==FPV_LOCK);
    tap(120,60);assert(!scout.scan_waterfall && scout.channel==20 && scout.scan.mode==FPV_LOCK);
    assert(samples_seen[18]==1 && samples_seen[20]>20 && samples_seen[22]>20 && samples_seen[39]>20);
    assert(scout_lq()==-1);
    scout_detect_channel();
    assert(scout_lq()==100);
    scripted=false;
    before=captures;cyd_ui_step();assert(captures==before+2);
    cyd_ui_step();assert(captures==before+3); /* video keeps running between detector checks */
    tap(180,300);assert(scout.scan.mode==FPV_SCAN && scout.skipped_mhz==5809);
    scout.channel=20;
    scout_shortlist((fpv_evidence_t){10,0,1});
    assert(!scout.candidate_count);
    int64_t held_at=clock_us;
    scout.hold_started=held_at;
    scout.scan=(fpv_scan_t){.mode=FPV_LOCK};
    clock_us=held_at+SCOUT_SYNC_TIMEOUT_US-1;
    prev_sweep=spectrum_calls;
    cyd_ui_step();assert(scout.scan.mode==FPV_LOCK && scout.channel==20);
    assert(spectrum_calls==prev_sweep); /* No panoramic retune while recovering. */
    clock_us++;
    video_rf=video_rows=video_sync=true;
    cyd_ui_step();assert(scout.scan.mode==FPV_LOCK && scout.channel==20);
    assert(scout.scan.last_sync==clock_us && spectrum_calls==prev_sweep+1);
    video_rows=video_sync=false; /* Raw IQ and cached rows are still available. */
    clock_us+=SCOUT_VIDEO_RECENT_US;
    cyd_ui_step();assert(scout.scan.mode==FPV_LOCK && scout.scan.last_sync==sv_v.last_row_us);
    assert(spectrum_calls==prev_sweep+1);
    clock_us=scout.scan.last_sync+SCOUT_SYNC_TIMEOUT_US-1;
    video_rows=true; /* An isolated line without a current pulse train earns no hold. */
    cyd_ui_step();assert(scout.scan.mode==FPV_LOCK && scout.scan.last_sync<sv_v.last_row_us);
    clock_us++;
    cyd_ui_step();assert(scout.scan.mode==FPV_SCAN && scout.channel==0 && !scout.skipped_mhz);
    video_rf=video_rows=false;
    tap(30,20);tap(120,210);
    tap(120,57);assert(scout.page==SCOUT_SETTINGS && !scout.auto_rescan && saved_rescan==0 && commits==1);
    memset(&scout,0,sizeof(scout));
    cyd_ui_init();assert(!scout.auto_rescan && scout.page==SCOUT_SCAN_PAGE);
    assert(cyd_ui_command("SCOUT?") && strstr(response,"rescan=OFF"));
    scout.channel=20;scout.manual=true;
    scout.scan=(fpv_scan_t){.mode=FPV_LOCK,.last_sync=clock_us};
    clock_us+=2*SCOUT_SYNC_TIMEOUT_US;
    cyd_ui_step();assert(scout.scan.mode==FPV_LOCK && scout.channel==20 && scout.manual);
    tap(30,20);tap(120,210);
    tap(120,57);assert(scout.auto_rescan && saved_rescan==1 && commits==2);
    tap(30,20);tap(120,92);
    scout.scan=(fpv_scan_t){.mode=FPV_SCAN};
    clock_us+=2*SCOUT_SYNC_TIMEOUT_US;
    cyd_ui_step();assert(scout.manual && scout.channel==20 && scout.scan.mode==FPV_SCAN);
    scout.scan=(fpv_scan_t){.mode=FPV_LOCK,.last_sync=clock_us};
    clock_us+=SCOUT_SYNC_TIMEOUT_US;
    cyd_ui_step();assert(!scout.manual && scout.scan.mode==FPV_SCAN && scout.channel==0);
    tap(30,20);tap(120,171);
    assert(scout.page==SCOUT_SPECTRUM && scout.waterfall && !scout.spectrum_held);
    before=captures;prev_sweep=spectrum_calls;
    cyd_ui_step();
    assert(captures==before && spectrum_calls==prev_sweep+1 && scout.waterfall_count==1 && scout.waterfall[0]==127);
    tap(50,300);clock_us+=1000;cyd_ui_step();
    assert(scout.spectrum_held && spectrum_calls==prev_sweep+1 && scout.waterfall_count==1);
    assert(cyd_ui_command("SCOUT?") && strstr(response,"mode=HOLD"));
    tap(210,300);
    assert(scout.spectrum_24 && !scout.spectrum_held && !scout.spectrum_valid && !scout.waterfall_count);
    clock_us+=1000;cyd_ui_step();
    assert(scout.waterfall_count==1 && captures==before);
    tap(30,20);
    tap(120,236);assert(scout.page==SCOUT_REMOTE_ID && rid_starts==1 && !scout.waterfall);
    cyd_ui_step();assert(rid_steps==1);
    assert(cyd_ui_command("SCOUT?") && strstr(response,"mode=REMOTEID active=1"));
    tap(30,20);assert(scout.page==SCOUT_MENU && rid_stops==1 && !scout_rid);
    assert(scout.page==SCOUT_MENU && !scout.waterfall);
    test_tuner(false);
    test_tuner(true);
    test_overlapping_channels();
    test_signal_log();
    test_fullscreen_video();
    puts("PASS: navigation and signal history in both orientations, manual tuning, live scan, top-three comparison, spectrum controls, and saved sync-loss rescan");
}
'''
with tempfile.TemporaryDirectory(prefix='scout-navigation-') as d:
    t=Path(d);(t/'esp_err.h').write_text('typedef int esp_err_t;\n');(t/'sdkconfig.h').write_text('#define CONFIG_CYD_C5_BAND_LINES 4\n');(t/'test.c').write_text(code)
    inc=[t,F,R/'espsoup/components/cyd_c5/include'];cl=shutil.which('cl')
    if cl:
        exe=t/'test.exe';cmd=[cl,'/nologo','/std:c11','/O2']+['/I'+str(p) for p in inc]+[str(t/'test.c'),'/Fe:'+str(exe),'/Fo:'+str(t)+os.sep]
    else:
        cc=shutil.which('cc') or shutil.which('gcc') or shutil.which('clang');assert cc,'Activate a host C compiler'
        exe=t/'test';cmd=[cc,'-std=c11','-O2']+['-I'+str(p) for p in inc]+[str(t/'test.c'),'-o',str(exe)]
    subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
