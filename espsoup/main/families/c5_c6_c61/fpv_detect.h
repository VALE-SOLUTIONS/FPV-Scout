/* SPDX-License-Identifier: GPL-3.0-or-later
 * Analog composite sync detector. Times below are in 0.5 us bins.
 * RF power alone never qualifies a signal. Snapshot gaps are not joined.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "demod_tab.h"

#define FPV_SAMPLES 16380u
#define FPV_BINS (FPV_SAMPLES / 10u)
typedef struct { unsigned h_pairs, v_pairs; int polarity; } fpv_evidence_t;

#ifdef SCOUT_RUST_CORE
extern fpv_evidence_t rust_fpv_sync(const int16_t *, unsigned, int);
extern fpv_evidence_t rust_fpv_detect(const uint32_t *, int16_t *, const uint8_t *);
#define fpv_sync rust_fpv_sync
static fpv_evidence_t fpv_detect(const uint32_t *w, int16_t *bins) {
    return rust_fpv_detect(w,bins,vf_atan_tab);
}
#else
static fpv_evidence_t fpv_sync(const int16_t *v, unsigned n, int pol) {
    fpv_evidence_t r = {0, 0, pol};
    int lo = 32767, hi = -32768;
    for (unsigned j = 0; j < n; j++) {
        int z = pol * v[j];
        if (z < lo) lo = z;
        if (z > hi) hi = z;
    }
    if (hi - lo < 20 || n < 256) return r;
    unsigned hist[64] = {0};
    for (unsigned j = 0; j < n; j++) hist[(pol * v[j] - lo) * 63 / (hi - lo)]++;
    unsigned total = 0, q03 = 0, q50 = 0;
    bool got03 = false;
    for (unsigned j = 0; j < 64; j++) {
        total += hist[j];
        if (!got03 && total >= (n * 3 + 99) / 100) { q03 = j; got03 = true; }
        if (total >= n / 2) { q50 = j; break; }
    }
    int tip = lo + (hi - lo) * (int)q03 / 63;
    int blank = lo + (hi - lo) * (int)q50 / 63;
    if (blank - tip < 16) return r;
    int th = tip + (blank - tip) * 40 / 100;
    int start = -1, prev_h = -1000, prev_v = -1000;
    unsigned chain = 0, best = 0;
    for (unsigned j = 0; j <= n; j++) {
        bool low = j < n && pol * v[j] < th;
        if (low && start < 0) start = (int)j;
        if (!low && start >= 0) {
            int width = (int)j - start;
            if (start > 0 && j < n) {
                /* H pulse 3.5..6 us, cadence 63..65 us (PAL or NTSC). */
                if (width >= 7 && width <= 12) {
                    int d = start - prev_h;
                    chain = d >= 126 && d <= 130 ? chain + 1 : 0;
                    if (chain > best) best = chain;
                    prev_h = start;
                }
                /* Vertical serration: 25..30 us low, repeats each half-line. */
                if (width >= 50 && width <= 60) {
                    int d = start - prev_v;
                    if (d >= 62 && d <= 66) r.v_pairs++;
                    prev_v = start;
                }
            }
            start = -1;
        }
    }
    r.h_pairs = best;
    return r;
}

/* Input is IQ10 at 20 MS/s, I low 10 bits and Q next 10 bits.
 * The phase table and amplitude scaling follow ESPsoup's VFM discriminator.
 */
static fpv_evidence_t fpv_detect(const uint32_t *w, int16_t *bins) {
    unsigned sum = 0, count = 0, shift = 0;
    for (unsigned j = 0; j < FPV_SAMPLES; j += 61, count++) {
        int i = (int)(w[j] & 1023u), q = (int)((w[j] >> 10) & 1023u);
        if (i > 511) i -= 1024;
        if (q > 511) q -= 1024;
        sum += (i < 0 ? -i : i) + (q < 0 ? -q : q);
    }
    while (shift < 4 && sum / count > (34u << shift)) shift++;
    unsigned prev = vf_atan_tab[((w[0] >> shift) & 63u) | ((w[0] >> (shift + 4)) & 4032u)];
    for (unsigned b = 0; b < FPV_BINS; b++) {
        int acc = 0;
        for (unsigned k = 0; k < 10; k++) {
            uint32_t x = w[b * 10 + k];
            unsigned p = vf_atan_tab[((x >> shift) & 63u) | ((x >> (shift + 4)) & 4032u)];
            acc += (int)((p - prev + 128u) & 255u) - 128;
            prev = p;
        }
        bins[b] = (int16_t)acc;
    }
    fpv_evidence_t a = fpv_sync(bins, FPV_BINS, 1), b = fpv_sync(bins, FPV_BINS, -1);
    return b.h_pairs + b.v_pairs > a.h_pairs + a.v_pairs ? b : a;
}

#endif
