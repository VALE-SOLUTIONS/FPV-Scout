/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../main/families/c5_c6_c61/scout_digital.h"

static uint32_t samples[16380];
static int16_t sine[1024];

static uint32_t pack16(double i, double q) {
    return (uint16_t)(int16_t)lround(i) | ((uint32_t)(uint16_t)(int16_t)lround(q) << 16);
}

static uint32_t next_sample(void) {
    static uint32_t rng = 1873;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng & 0xfffffu;
}

static void make_symbols(unsigned lag, unsigned period) {
    for (unsigned i = 0; i < 16380; i++) samples[i] = next_sample();
    for (unsigned index = 37; index + period <= 16380; index += period)
        for (unsigned i = 0; i < period - lag; i++) samples[index + i] = samples[index + i + lag];
}

int main(int argc, char **argv) {
    for (unsigned i = 0; i < 1024; i++) sine[i] = (int16_t)lround(16383 * sin(i * 6.283185307179586 / 1024));
    if (argc == 3 && !strcmp(argv[2],"elrs")) {
        FILE *file = fopen(argv[1],"rb");
        if (!file) return 2;
        unsigned counts[9] = {0}, total = 0;
        while (fread(samples,1600*sizeof(samples[0]),1,file) == 1) {
            unsigned sf = scout_elrs_chirps(samples,1600,sine,NULL);
            assert(sf <= 8);
            counts[sf]++;
            total++;
        }
        fclose(file);
        printf("%u windows, SF5=%u SF6=%u SF7=%u SF8=%u\n",total,counts[5],counts[6],counts[7],counts[8]);
        return 0;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1],"rb");
        if (!file) return 2;
        unsigned total = 0, o4 = 0, avatar = 0;
        while (fread(samples,sizeof(samples),1,file) == 1) {
            uint32_t match = scout_rf_timing(samples,16380,SCOUT_TIMING_O4 | SCOUT_TIMING_AVATAR,NULL);
            total++;
            if (match & SCOUT_TIMING_O4) o4++;
            if (match & SCOUT_TIMING_AVATAR) avatar++;
        }
        fclose(file);
        printf("%u snapshots, O4 timing %u, Avatar timing %u\n",total,o4,avatar);
        return 0;
    }

    int16_t bins[256] = {0};
    scout_rf_shape_t shape;
    bins[128] = 5000;
    scout_rf_shape(bins,&shape);
    assert(shape.width_khz == 0);
    for (unsigned i = 64; i < 192; i++) bins[i] = 2000;
    scout_rf_shape(bins,&shape);
    assert(shape.width_khz == 20000 && shape.offset_khz == 0 && shape.contrast == 2000);
    for (unsigned i = 0; i < 16380; i++) samples[i] = next_sample();
    scout_rf_quality_t quality;
    assert(scout_rf_timing(samples,16380,3,&quality) == 0 && !quality.power && !quality.coherence);
    make_symbols(1333,1427);
    assert(scout_rf_timing(samples,16380,3,&quality) == SCOUT_TIMING_O4);
    assert(quality.power > 0 && quality.coherence >= 68 && quality.coherence <= 100);
    make_symbols(3657,4114);
    assert(scout_rf_timing(samples,16380,3,NULL) == SCOUT_TIMING_AVATAR);
    make_symbols(2667,2854);
    assert(scout_rf_timing(samples,16380,7,NULL) == SCOUT_TIMING_DJI_15);
    assert(scout_rf_timing(samples,4000,3,NULL) == 0);
    for (unsigned i = 0; i < 16380; i++) samples[i] = 123;
    assert(scout_rf_timing(samples,16380,3,NULL) == 0);
    for (unsigned sf = 5; sf <= 8; sf++) {
        const double period = (1u << sf)*2000000.0/812500.0;
        for (unsigned i = 0; i < 1600; i++) {
            const double t = fmod(i+28,period);
            const double angle = 6.283185307179586*(0.203125*(t*t/period-t)+0.025*i);
            samples[i] = pack16(14000*cos(angle) + (int)(next_sample()%801)-400,
                                14000*sin(angle) + (int)(next_sample()%801)-400);
        }
        assert(scout_elrs_chirps(samples,1600,sine,&quality) == sf);
        assert(quality.power > 0 && quality.coherence >= 76 && quality.coherence <= 100);
    }
    for (unsigned i = 0; i < 1600; i++)
        samples[i] = pack16(15000*cos(0.32*i),15000*sin(0.32*i));
    assert(scout_elrs_chirps(samples,1600,sine,&quality) == 0 && !quality.power && !quality.coherence);
    for (unsigned i = 0; i < 1600; i++) samples[i] = next_sample()<<6;
    assert(scout_elrs_chirps(samples,1600,sine,NULL) == 0);

    scout_rf_track_t track = {0};
    const scout_rf_hit_t o4 = {5795, 20000, 1900, SCOUT_RF_O4};
    track.best = o4;
    assert(!scout_rf_finish_sweep(&track) && !track.shown.kind);
    track.best = o4;
    assert(scout_rf_finish_sweep(&track) && track.shown.kind == SCOUT_RF_UNKNOWN);
    track.best = o4;
    assert(scout_rf_finish_sweep(&track) && track.shown.kind == SCOUT_RF_O4);
    track.best = (scout_rf_hit_t){5794, 20000, 1800, SCOUT_RF_UNKNOWN};
    assert(!scout_rf_finish_sweep(&track) && track.shown.kind == SCOUT_RF_O4);
    track.best = (scout_rf_hit_t){5794, 20000, 1800, SCOUT_RF_UNKNOWN};
    assert(scout_rf_finish_sweep(&track) && track.shown.kind == SCOUT_RF_UNKNOWN);
    assert(!scout_rf_finish_sweep(&track));
    assert(scout_rf_finish_sweep(&track) && track.shown.kind == SCOUT_RF_NONE);
    puts("PASS: RF shape, synthetic O4 and Avatar timing, noise rejection, repeat evidence and alert expiry");
}
