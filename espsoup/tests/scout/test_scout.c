#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../main/families/c5_c6_c61/fpv_scan.h"
static uint32_t words[FPV_SAMPLES];
static int16_t bins[FPV_BINS];
static unsigned rng = 42;
static double noise(void) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0 * 2 - 1; }
static unsigned pack(int i, int q) { return ((unsigned)i & 1023u) | (((unsigned)q & 1023u) << 10); }
static void video(double offset, int pal, int polarity, double grain, int vertical) {
    double phase = 0, T = pal ? 64e-6 : 1.0 / 15734.264;
    for (unsigned j=0;j<FPV_SAMPLES;j++) {
        double t = offset + j / 20e6;
        double local = fmod(t,T), half = fmod(t,T/2);
        double level = local < 4.7e-6 ? -1.0 : local < 10e-6 ? 0.0 : 0.3 + 0.6 * fmod(t * 2511,1.0);
        if (vertical && t < (pal?5:6) * T/2) level = half < T/2 - 4.7e-6 ? -1.0 : 0.0;
        phase += 6.283185307179586 * (0.35e6 + polarity * 1.0e6 * level) / 20e6;
        words[j] = pack((int)(150*cos(phase)+grain*noise()),(int)(150*sin(phase)+grain*noise()));
    }
}
int main(void) {
    for (int pal=0;pal<=1;pal++) for(int pol=-1;pol<=1;pol+=2) for(int grain=0;grain<=12;grain+=12) {
        fpv_scan_t s = {0};
        for(int k=0;k<3;k++) {
            video(.004+k*.001,pal,pol,grain,1);
            fpv_evidence_t e=fpv_detect(words,bins);
            assert(e.h_pairs>=4);
            assert(!fpv_observe(&s,e,k*10000));
        }
        assert(s.mode==FPV_VERIFY);
        video(0,pal,pol,grain,1);
        fpv_evidence_t e=fpv_detect(words,bins);
        printf("%s pol%d noise%d: H%u V%u\n",pal?"PAL":"NTSC",pol,grain,e.h_pairs,e.v_pairs);
        assert(e.v_pairs>=2);
        assert(fpv_observe(&s,e,40000));
        assert(s.mode==FPV_HIT);
        s.mode=FPV_LOCK;
        assert(!fpv_observe(&s,(fpv_evidence_t){0},2000000));
        assert(s.mode==FPV_LOCK);
        fpv_resume(&s); assert(s.mode==FPV_SCAN && !s.h_bursts && !s.v_bursts);
    }
    for(int k=0;k<1000;k++) {
        for(unsigned j=0;j<FPV_SAMPLES;j++) {
            if(k<500) words[j]=pack((int)(200*noise()),(int)(200*noise()));
            else { double p=j*.13; words[j]=pack((int)(150*cos(p)+5*noise()),(int)(150*sin(p)+5*noise())); }
        }
        fpv_evidence_t e=fpv_detect(words,bins);
        assert(e.h_pairs<4 && e.v_pairs<2);
    }
    fpv_scan_t s={0};
    for(int k=0;k<100;k++) {
        video(.004,1,1,0,0);
        fpv_evidence_t e=fpv_detect(words,bins);
        assert(!fpv_observe(&s,e,k*1000));
    }
    assert(s.mode==FPV_VERIFY && !s.v_bursts);
    fpv_resume(&s);
    assert(!fpv_observe(&s,(fpv_evidence_t){7,0,1},0));
    assert(!fpv_observe(&s,(fpv_evidence_t){7,3,-1},10000));
    assert(!fpv_observe(&s,(fpv_evidence_t){7,3,-1},20000));
    assert(s.mode==FPV_VERIFY);
    assert(!fpv_observe(&s,(fpv_evidence_t){7,3,1},400000));
    assert(s.mode==FPV_SCAN && !s.h_bursts && !s.v_bursts);
    puts("PASS: PAL/NTSC, both polarities, noise, tone rejection, H-only rejection, lock/resume, polarity isolation");
    return 0;
}
