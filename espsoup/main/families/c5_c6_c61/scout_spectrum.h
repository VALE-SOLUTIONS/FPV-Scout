/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <limits.h>
#include "scout_layout.h"
#define SCOUT_BAND_LOW 5362
#define SCOUT_BAND_MIDDLE 5800
#define SCOUT_BAND_HIGH 5945
#define SCOUT_SWEEP_KEEP 30
#define SCOUT_SWEEP_STEPS ((SCOUT_BAND_HIGH-SCOUT_BAND_LOW+SCOUT_SWEEP_KEEP-1)/SCOUT_SWEEP_KEEP)
#ifdef SCOUT_RUST_CORE
extern int rust_scout_frequency_x(int);
extern void rust_scout_spectrum_add(int16_t *,unsigned,const int16_t *);
extern int32_t rust_scout_signal_score(const int16_t *);
#define scout_frequency_x rust_scout_frequency_x
#define scout_spectrum_add rust_scout_spectrum_add
#define scout_signal_score rust_scout_signal_score
#else
/* Two linear halves put 5800 MHz at the physical midpoint without relabeling data. */
static int scout_frequency_x(int khz) {
    int middle=SCOUT_SPEC_W/2;
    if(khz<=SCOUT_BAND_LOW*1000)return 0;
    if(khz>=SCOUT_BAND_HIGH*1000)return SCOUT_SPEC_W-1;
    if(khz<=SCOUT_BAND_MIDDLE*1000)
        return ((khz-SCOUT_BAND_LOW*1000)*middle+(SCOUT_BAND_MIDDLE-SCOUT_BAND_LOW)*500)/((SCOUT_BAND_MIDDLE-SCOUT_BAND_LOW)*1000);
    return middle+((khz-SCOUT_BAND_MIDDLE*1000)*(SCOUT_SPEC_W-1-middle)+(SCOUT_BAND_HIGH-SCOUT_BAND_MIDDLE)*500)/((SCOUT_BAND_HIGH-SCOUT_BAND_MIDDLE)*1000);
}
static void scout_spectrum_add(int16_t *plot,unsigned centre,const int16_t *bins) {
    for(int j=32;j<224;j++) {
        int khz=(int)centre*1000+(j-128)*40000/256;
        if(khz<SCOUT_BAND_LOW*1000 || khz>SCOUT_BAND_HIGH*1000)continue;
        int x=scout_frequency_x(khz);
        if(bins[j]>plot[x])plot[x]=bins[j];
    }
}

static int32_t scout_signal_score(const int16_t *bins) {
    int32_t score = INT32_MIN;
    for (int i = 64; i <= 192; i++) {
        int offset = i < 128 ? 128 - i : i - 128;
        if (offset < 2 || bins[i] == INT16_MIN) continue;
        int32_t centered = bins[i] - 16 * offset;
        if (centered > score) score = centered;
    }
    return score;
}

#endif
