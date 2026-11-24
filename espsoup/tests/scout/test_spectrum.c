/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../main/families/c5_c6_c61/scout_spectrum.h"
int main(void) {
    assert(scout_frequency_x(SCOUT_BAND_LOW*1000)==0);
    assert(scout_frequency_x(SCOUT_BAND_MIDDLE*1000)==SCOUT_SPEC_W/2);
    assert(scout_frequency_x(SCOUT_BAND_HIGH*1000)==SCOUT_SPEC_W-1);
    int previous=-1;
    for(int f=5300000;f<=6000000;f+=1000) {
        int x=scout_frequency_x(f);assert(x>=previous && x>=0 && x<SCOUT_SPEC_W);previous=x;
    }
    struct { int16_t before,plot[SCOUT_SPEC_W],after; } data;
    data.before=1234;data.after=2345;
    for(unsigned x=0;x<SCOUT_SPEC_W;x++)data.plot[x]=INT16_MIN;
    for(unsigned step=0;step<SCOUT_SWEEP_STEPS;step++) {
        unsigned centre=SCOUT_BAND_LOW+SCOUT_SWEEP_KEEP/2+step*SCOUT_SWEEP_KEEP;
        int16_t bins[256];
        for(int j=0;j<256;j++) {
            int khz=(int)centre*1000+(j-128)*40000/256;
            bins[j]=(khz>5799900 && khz<5800100)?-100:-3000;
        }
        scout_spectrum_add(data.plot,centre,bins);
    }
    unsigned filled=0;
    for(unsigned x=0;x<SCOUT_SPEC_W;x++)filled+=data.plot[x]!=INT16_MIN;
    printf("Full-band sweep: %u/%u columns, %u chunks\n",filled,SCOUT_SPEC_W,SCOUT_SWEEP_STEPS);
    assert(filled==SCOUT_SPEC_W);
    assert(data.plot[SCOUT_SPEC_W/2]==-100);
    assert(data.before==1234 && data.after==2345);
    int16_t direct[256], alias[256];
    for (int i=0;i<256;i++) direct[i]=alias[i]=INT16_MIN;
    assert(scout_signal_score(direct)==INT32_MIN);
    direct[128]=INT16_MAX; /* Ignore the DC spike. */
    direct[131]=-1000;
    alias[166]=-900;
    assert(scout_signal_score(direct)==-1048);
    assert(scout_signal_score(direct)>scout_signal_score(alias));
    alias[130]=-600;
    assert(scout_signal_score(alias)>scout_signal_score(direct));
    puts("PASS: spectrum mapping, full coverage, bounds and center-weighted signal ranking");
}
