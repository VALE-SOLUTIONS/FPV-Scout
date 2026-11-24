#!/usr/bin/env python3
"""Check the actual sweep helper restores RF settings on success and failure."""
from pathlib import Path
import os,shutil,subprocess,tempfile
R=Path(__file__).resolve().parents[1]
F=R/'espsoup/main/families/c5_c6_c61'
s=(F/'cyd_ui.h').read_text()
state=s[s.index('typedef struct { const char *name;'):s.index('} scout;')+len('} scout;')]
sweep=s[s.index('static void scout_spectrum_step('):s.index('static bool scout_update_spectrum(')]
code='''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "scout_navigation.h"
#include "scout_spectrum.h"
#include "scout_digital.h"
#include "scout_signal_log.h"
#include "fpv_scan.h"
static unsigned frequency_mhz=5658,rx_channel_mode=20,prepares,captures;
static int rx_analog_filter=77;
static bool rx_ready=true,fail_all;
static int16_t spec_bins[1024];
static void prepare_rx(void){prepares++;rx_ready=true;}
static unsigned rx_bandwidth_phy_mode(unsigned n){return n;}
static int rx_bandwidth_dcap(unsigned n){return n+100;}
static const char *spec_compute(unsigned,unsigned,unsigned,int16_t*,uint32_t*);
'''+state+sweep+'''
static const char *spec_compute(unsigned rate,unsigned fft,unsigned avg,int16_t *bins,uint32_t *us) {
    captures++;
    assert(rate==1 && fft==256 && avg==4);
    const unsigned low=scout.spectrum_24 || scout.page==SCOUT_CONTROL ? SCOUT_BAND_24_LOW:SCOUT_BAND_LOW;
    assert(frequency_mhz==low+15+scout.spectrum_step*30);
    assert(rx_channel_mode==40 && rx_analog_filter==140 && rx_ready);
    for(unsigned j=0;j<256;j++)bins[j]=-3000;
    *us=10;
    return fail_all || scout.spectrum_step==5 ? "capture_error" : NULL;
}
int main(void) {
    for(int mode=0;mode<2;mode++) {
        memset(&scout,0,sizeof(scout));prepares=captures=0;fail_all=mode;
        for(unsigned step=0;step<SCOUT_SWEEP_STEPS;step++) {
            scout_spectrum_step(step*20000);
            assert(frequency_mhz==5658 && rx_channel_mode==20 && rx_analog_filter==77 && rx_ready);
        }
        assert(prepares==2*SCOUT_SWEEP_STEPS && captures==SCOUT_SWEEP_STEPS);
        assert(scout.spectrum_step==0 && scout.spectrum_valid==!fail_all);
        if(fail_all)for(unsigned x=0;x<SCOUT_SPEC_W;x++)assert(scout.spectrum_plot[x]==INT16_MIN);
    }
    for(int control=0;control<2;control++) {
        memset(&scout,0,sizeof(scout));prepares=captures=0;fail_all=false;
        scout.page=control?SCOUT_CONTROL:SCOUT_SPECTRUM;
        scout.spectrum_24=!control;
        for(unsigned step=0;step<3;step++) {
            scout_spectrum_step(step*20000);
            assert(frequency_mhz==5658 && rx_channel_mode==20 && rx_analog_filter==77 && rx_ready);
        }
        assert(prepares==6 && captures==3 && scout.spectrum_step==0 && scout.spectrum_valid);
        for(unsigned x=0;x<SCOUT_SPEC_W;x++)assert(scout.spectrum_plot[x]==-3000);
    }
    memset(&scout,0,sizeof(scout));
    scout.page=SCOUT_SCAN_PAGE;
    for(unsigned sweep=1;sweep<=3;sweep++) {
        scout.spectrum_step=SCOUT_SWEEP_STEPS-1;
        scout.digital.best=(scout_rf_hit_t){5800,18000,2100,SCOUT_RF_O4};
        scout_spectrum_step(sweep*1000000);
        assert(scout_log.count==(sweep>1));
    }
    assert(scout_log.entries[0].kind==SCOUT_LOG_O4 && scout_log.entries[0].seen_seconds==3);
    scout.spectrum_step=SCOUT_SWEEP_STEPS-1;
    scout_spectrum_step(4000000);
    assert(scout_log.entries[0].seen_seconds==3);
    puts("PASS: both RF bands, complete 2.4 GHz coverage, receive settings restored on success/error, missing-data handling");
}
'''
with tempfile.TemporaryDirectory(prefix='scout-sweep-') as d:
    t=Path(d);(t/'esp_err.h').write_text('typedef int esp_err_t;\n');(t/'sdkconfig.h').write_text('#define CONFIG_CYD_C5_BAND_LINES 4\n');(t/'test.c').write_text(code)
    inc=[t,F,R/'espsoup/components/cyd_c5/include'];cl=shutil.which('cl')
    if cl:
        exe=t/'test.exe';cmd=[cl,'/nologo','/std:c11','/O2']+['/I'+str(p) for p in inc]+[str(t/'test.c'),'/Fe:'+str(exe),'/Fo:'+str(t)+os.sep]
    else:
        cc=shutil.which('cc') or shutil.which('gcc') or shutil.which('clang');assert cc,'Activate a host C compiler'
        exe=t/'test';cmd=[cc,'-std=c11','-O2']+['-I'+str(p) for p in inc]+[str(t/'test.c'),'-o',str(exe)]
    subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
