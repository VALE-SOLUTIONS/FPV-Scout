/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "fpv_detect.h"
enum { FPV_SCAN, FPV_VERIFY, FPV_HIT, FPV_LOCK };
typedef struct {
    unsigned mode, h_bursts, v_bursts;
    int polarity;
    int64_t deadline, last_sync;
} fpv_scan_t;
#ifdef SCOUT_RUST_CORE
extern void rust_fpv_resume(fpv_scan_t *);
extern bool rust_fpv_observe(fpv_scan_t *, fpv_evidence_t, int64_t);
#define fpv_resume rust_fpv_resume
#define fpv_observe rust_fpv_observe
_Static_assert(sizeof(fpv_scan_t)==32, "Rust scan ABI size");
_Static_assert(sizeof(fpv_evidence_t)==12, "Rust evidence ABI size");
#else
static void fpv_resume(fpv_scan_t *s) { *s = (fpv_scan_t){0}; }
static bool fpv_observe(fpv_scan_t *s, fpv_evidence_t e, int64_t now) {
    if (s->mode == FPV_VERIFY && now >= s->deadline) {
        fpv_resume(s);
        return false;
    }
    bool h = e.h_pairs >= 4;
    if (s->mode == FPV_HIT || s->mode == FPV_LOCK) {
        if (h && e.polarity == s->polarity) s->last_sync = now;
        return false;
    }
    if (s->mode == FPV_SCAN) {
        if (!h) return false;
        s->mode = FPV_VERIFY;
        s->polarity = e.polarity;
        s->deadline = now + 400000;
    }
    if (e.polarity == s->polarity) {
        if (h) s->h_bursts++;
        if (e.v_pairs >= 2) s->v_bursts++;
    }
    if (s->h_bursts >= 3 && s->v_bursts) {
        s->mode = FPV_HIT;
        s->last_sync = now;
        return true;
    }
    return false;
}

#endif
