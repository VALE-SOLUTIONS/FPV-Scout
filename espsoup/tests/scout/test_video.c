/* SPDX-License-Identifier: GPL-3.0-only */
/* Synthetic PAL/NTSC tests of the adapted C5VRX LCD decoder. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define HALF_WORDS 16384u
static uint32_t bank[2*HALF_WORDS];
#define DUMP_BUF bank
static int64_t opt_last_t0;
static int64_t now_us;
static int pal, polarity;
static double start_us;
static unsigned drawn;
static bool blank_signal, weak_field_sync, bad_line_sync, verify_picture;
static bool verify_interpolation;
static bool progressive;
static bool changed_scene;
static uint8_t picture[320*180];
enum { MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2 };
static void *heap_caps_malloc(size_t n,unsigned caps) {
    assert(n<=sizeof(picture) && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    return picture;
}
static const char *fast_capture(unsigned n,unsigned div,uint32_t *elapsed);
static int64_t esp_timer_get_time(void) { return now_us; }
#include "../../main/families/c5_c6_c61/scout_video.h"
static uint16_t band[CYD_MAX_W*CYD_BAND_LINES];
static uint16_t lcd_center[SCOUT_MAX_VIDEO_H];
static bool fullscreen;
static bool landscape;
bool cyd_landscape(void) { return landscape; }
uint16_t *cyd_band(void) { return band; }
void cyd_blit(int x,int y,int w,int h,const uint16_t *px) {
    if (fullscreen) {
        const int full_h = cyd_landscape() ? cyd_height() : SCOUT_VIDEO_H, top = (cyd_height()-full_h)/2;
        assert(x==0 && y>=top && y+h<=top+full_h && w==cyd_width() && h<=CYD_BAND_LINES);
        for (int r=0;r<h;++r) {
            const int source=(y+r-top)*SCOUT_VIDEO_H/full_h;
            assert(px[r*w+w-1]==sv_grey565(sv_picture[source*w+w-1]));
        }
        drawn+=h;
        return;
    }
    assert(x==SCOUT_VIDEO_X && y>=SCOUT_VIDEO_Y && y+h<=SCOUT_VIDEO_Y+SCOUT_VIDEO_H && w==SCOUT_VIDEO_W && h>0 && h<=CYD_BAND_LINES);
    for (int row=0;row<h;row++) lcd_center[y-SCOUT_VIDEO_Y+row]=px[row*w+w/2];
    if (verify_picture)
        for (int pixel=0;pixel<w*h;pixel++)
            assert(px[pixel]==sv_grey565(picture[(y-SCOUT_VIDEO_Y)*w+pixel]));
    if (verify_interpolation && y==SCOUT_VIDEO_Y) {
        assert(h>=3);
        const uint8_t expected[] = {40,100,160};
        for (int row=0;row<3;row++)
            for (int x=0;x<w;x++) assert(px[row*w+x]==sv_grey565(expected[row]));
    }
    assert(px==band);drawn+=h;
}
static const char *fast_capture(unsigned n,unsigned div,uint32_t *elapsed) {
    assert(n==sv_SNAP && div==2);
    /* Live F4 replay showed a measured NTSC field about 0.125% short. */
    double phase=0,T=pal?64.0:0.99875*1000000.0/15734.264;
    double field=((pal?312.5:262.5)-(progressive?0.5:0.0))*T, part=(pal?2.5:3.0)*T;
    for(unsigned j=0;j<n;j++) {
        double t=fmod(start_us+j/(double)sv_RATE,field),line=fmod(t,T),half=fmod(t,T/2);
        double level;
        if(t<part || (t>=2*part && t<3*part)) level=half<2.35?-1:0;
        else if(t<2*part) level=half<T/2-4.7?-1:0;
        else level=line<4.7?(bad_line_sync?-3.5:-1):line<10?0:(t<field/2)!=changed_scene?0.15:0.75;
        if (weak_field_sync && t<3*part) level=0;
        if (blank_signal) level=0;
        phase+=2*M_PI*(0.2+polarity*level)/sv_RATE;
        int i=(int)(400*cos(phase)),q=(int)(400*sin(phase));
        bank[j]=((unsigned)i&1023u)|(((unsigned)q&1023u)<<10);
    }
    opt_last_t0=(int64_t)start_us;
    *elapsed=n/sv_RATE;
    now_us=opt_last_t0+*elapsed;
    return NULL;
}
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    assert(sv_iq_phase(0)==0 && sv_iq_phase(511)==0 && sv_iq_phase(512)==512);
    assert(sv_iq_phase(511u<<10)==256 && sv_iq_phase(512u<<10)==768);
    assert(sv_iq_phase(512u|(512u<<10))==640 && sv_iq_phase(511u|(511u<<10))==128);
    for(int rotation=0;rotation<2;rotation++) {
        landscape=rotation!=0;
        scout_video_init();
        memset(sv_v.row_seen,0xff,sizeof(sv_v.row_seen));
        sv_v.rows=SCOUT_VIDEO_H;
        sv_v.last_row_us=100000;
        for(int i=0;i<SCOUT_VIDEO_W*SCOUT_VIDEO_H;i++) picture[i]=(uint8_t)(i*37);
        drawn=0;verify_picture=true;
        scout_video_render(100000);
        assert(drawn==(unsigned)SCOUT_VIDEO_H);
        sv_v.last_row_us=200000;
        scout_video_render(200000);
        assert(drawn==(unsigned)SCOUT_VIDEO_H);
        sv_v.row_dirty[0]=0x24; /* Two isolated rows, with clean rows between them. */
        scout_video_render(200000);
        assert(drawn==(unsigned)SCOUT_VIDEO_H+2);
        assert(!sv_v.row_dirty[0]);
        verify_picture=false;
        fullscreen=true;
        drawn=0;
        scout_video_invalidate();
        scout_video_render_mode(220000,"5769 MHz   RSSI +35 dB   LQ 100%");
        assert(drawn==(unsigned)(landscape?cyd_height():SCOUT_VIDEO_H));
        drawn=0;
        scout_video_render_mode(250000,"5769 MHz   RSSI +34 dB   LQ 100%");
        assert(drawn==20);
        fullscreen=false;
        scout_video_reset();
        sv_v.rows=2;
        sv_v.row_seen[0]=5; /* Valid rows 0 and 2. */
        sv_v.last_row_us=200000;
        memset(picture,40,SCOUT_VIDEO_W);
        memset(picture+SCOUT_VIDEO_W,255,SCOUT_VIDEO_W);
        memset(picture+2*SCOUT_VIDEO_W,160,SCOUT_VIDEO_W);
        verify_interpolation=true;
        scout_video_render(200000);
        verify_interpolation=false;
    }
    scout_video_reset();
    memset(sv_s_u2,0,sv_U2N*sizeof(*sv_s_u2));
    assert(sv_active_video_quality(0,0,500,false)==sv_LINE_CLEAN);
    for (unsigned i=94;i<114;i++) sv_s_u2[i]=i%2 ? 230 : 0;
    assert(sv_active_video_quality(0,0,500,false)==sv_LINE_REJECTED);
    for (unsigned i=0;i<sv_U2N;i++) sv_s_u2[i]=(int32_t)i*20;
    sv_s_u2[95]+=230; /* A brief phase error in the first pixels of an otherwise valid line. */
    sv_v.S16=-500*16;
    sv_v.rows=1;sv_v.row_seen[0]=1;
    memset(picture,120,SCOUT_VIDEO_W);
    sv_draw_line(0,0,0,0,500,false);
    assert(picture[0]==120 && picture[SCOUT_VIDEO_W-1]==40);
    sv_s_u2[95]-=230;
    sv_s_u2[621]+=230;
    assert(sv_active_video_quality(0,0,500,false)==sv_LINE_CLEAN);
    memset(picture,120,SCOUT_VIDEO_W);
    sv_draw_line(0,512,0,0,500,true);
    assert(picture[0]==40 && picture[SCOUT_VIDEO_W-1]==120);
    assert(sv_preferred_source_line(0,240,118)==1);
    assert(sv_fallback_source_due(0,100));
    sv_v.preferred_ms[0]=110;
    assert(!sv_fallback_source_due(0,120));
    assert(!sv_fallback_source_due(0,850));
    assert(sv_fallback_source_due(0,860));
    sv_v.hsync16=sv_NTSC_LINE16;
    sv_v.hsync_reads=4;
    sv_s_np=5;
    for (unsigned j=0;j<sv_s_np;j++) sv_s_p[j]=(sv_pulse_t){.edge=2000+1271*(int)j,.cls=sv_P_H};
    sv_s_p[2].edge += 5;
    assert(abs(sv_line_offset(2)+5*256)<64);
    sv_v.tv_ok=true;sv_v.tv=1000;
    assert(!sv_track_standard(true,8) && !sv_v.pal && sv_v.tv_ok && sv_v.tv==1000);
    assert(sv_track_standard(false,8) && !sv_v.format_votes);
    assert(!sv_track_standard(true,8));
    assert(!sv_track_standard(true,8));
    assert(sv_track_standard(true,8) && sv_v.pal && !sv_v.tv_ok);
    sv_v.first_row_us=1000;
    sv_v.last_row_us=250000;
    sv_v.rows=(SCOUT_VIDEO_H*9+9)/10;
    assert(!scout_video_live(sv_v.last_row_us));
    sv_v.rows=(SCOUT_VIDEO_H*19+19)/20;
    assert(scout_video_live(sv_v.last_row_us));
    sv_v.rows=(SCOUT_VIDEO_H*4+4)/5;
    sv_v.last_row_us=sv_v.first_row_us+sv_PARTIAL_PREVIEW_WAIT_US;
    assert(scout_video_live(sv_v.last_row_us));
    for(int rotation=0;rotation<2;rotation++) for(int format=0;format<2;format++)
    for(pal=0;pal<=1;pal++) for(polarity=-1;polarity<=1;polarity+=2) {
        landscape=rotation!=0;
        progressive=format!=0;
        drawn=0;scout_video_init();scout_video_set_polarity(polarity);bank[2*HALF_WORDS-1]=0xf00dcafe;
        if (!rotation && !format) {
            uint32_t elapsed;
            scout_video_set_polarity(-polarity);
            start_us=10000.0;
            assert(!fast_capture(sv_SNAP,2,&elapsed));
            assert(scout_video_quality(polarity)>=6);
            assert(sv_v.pol==-polarity && !sv_v.rows && !sv_v.tv_ok);
            blank_signal=true;
            assert(!fast_capture(sv_SNAP,2,&elapsed));
            assert(scout_video_quality(polarity)==0);
            blank_signal=false;
            scout_video_set_polarity(polarity);
        }
        bool sparse_frame=false;
        for(unsigned k=0;k<600;k++) {
            start_us=1000000.0+k*(sv_SNAP/(double)sv_RATE+1100.0);
            assert(scout_video_capture());
            if (scout_video_live(now_us)) {
                unsigned before=drawn;
                scout_video_render(now_us);
                if (drawn>before) {
                    assert(drawn-before<=(unsigned)SCOUT_VIDEO_H);
                    if(sv_v.rows<(unsigned)SCOUT_VIDEO_H)sparse_frame=true;
                }
            }
        }
        printf("%s %s pol%d drawn=%u PAL=%u\n",landscape?"landscape":"portrait",pal?"PAL":"NTSC",polarity,drawn,sv_v.pal);
        printf("tv=%u lv=%u sync=%lld pol=%d hper=%d pulses=%u\n",sv_v.tv_ok,sv_v.lv_ok,(long long)sv_v.last_sync_us,sv_v.pol,sv_v.hper16,sv_s_np);
        assert(drawn>100 && sparse_frame);
        assert(sv_v.pal==(bool)pal && sv_v.field_timed);
        assert(sv_v.half_lines==(unsigned)((pal?625:525)-(progressive?1:0)));
        int expected=pal?sv_PAL_LINE16:20313;
        assert(abs(sv_v.hper16-expected)<=3);
        assert(bank[2*HALF_WORDS-1]==0xf00dcafe);
        assert(scout_video_live(now_us));
        const int width=SCOUT_VIDEO_W, height=SCOUT_VIDEO_H;
        int top=sv_next_row(height/4,height), bottom=sv_next_row(height*3/4,height);
        assert(top<height/2 && bottom<height);
        assert(picture[bottom*width+width/2]>picture[top*width+width/2]+35);
        if (progressive && !pal) {
            uint8_t before[SCOUT_MAX_VIDEO_H];
            uint16_t before_lcd[SCOUT_MAX_VIDEO_H];
            for (int row=0;row<height;row++) {
                before[row]=picture[row*width+width/2];
                before_lcd[row]=lcd_center[row];
            }
            changed_scene=true;
            const double next_capture=now_us+2000.0;
            for (unsigned k=0;k<120;k++) {
                start_us=next_capture+k*(sv_SNAP/(double)sv_RATE+1100.0);
                assert(scout_video_capture());
                scout_video_render(now_us);
            }
            assert(picture[top*width+width/2]>picture[bottom*width+width/2]+35);
            unsigned changed=0, posted=0;
            for (int row=0;row<height;row++) {
                changed+=abs((int)before[row]-picture[row*width+width/2])>35;
                posted+=lcd_center[row]!=before_lcd[row];
            }
            assert(changed>=(unsigned)height * 9 / 10);
            assert(posted>=(unsigned)height * 9 / 10);
            changed_scene=false;
        }
        const int64_t anchor=sv_v.tv;
        weak_field_sync=true;
        bad_line_sync=true;
        const int64_t clean_row=sv_v.last_row_us;
        for (int k=0;k<4;k++) {
            start_us=(double)(anchor/sv_RATE+600000+k*2000);
            assert(scout_video_capture() && sv_v.tv_ok && sv_v.last_row_us==clean_row);
        }
        bad_line_sync=false;
        for (int k=0;k<4;k++) {
            start_us=(double)(anchor/sv_RATE+700000+k*2000);
            assert(scout_video_capture() && sv_v.tv_ok && sv_v.tv==anchor);
        }
        assert(sv_v.last_row_us>clean_row && scout_video_live(now_us));
        weak_field_sync=false;
        blank_signal=true;
        const int64_t last_row=sv_v.last_row_us;
        start_us=(double)(anchor/sv_RATE+sv_FIELD_TRACK_US+2000);
        assert(scout_video_capture() && !sv_v.tv_ok && scout_video_live(now_us));
        unsigned before=drawn;
        scout_video_render(now_us);
        scout_video_render(now_us+66000);
        assert(drawn-before<=(unsigned)height);
        start_us=(double)(last_row+sv_FRAME_HOLD_US);
        assert(scout_video_capture() && !scout_video_live(now_us) && sv_v.rows==0);
        scout_video_reset();assert(!scout_video_live(now_us));
        blank_signal=false;
    }
    puts("PASS: PAL/NTSC timing in both orientations and polarities, progressive fields, impulse rejection and format stability");
}
