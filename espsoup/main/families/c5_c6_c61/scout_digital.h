/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { SCOUT_RF_NONE, SCOUT_RF_UNKNOWN, SCOUT_RF_O4, SCOUT_RF_AVATAR };
enum { SCOUT_TIMING_O4 = 1, SCOUT_TIMING_AVATAR = 2, SCOUT_TIMING_DJI_15 = 4 };

typedef struct {
    int32_t offset_khz;
    uint32_t width_khz, contrast;
} scout_rf_shape_t;

typedef struct { uint32_t power, coherence; } scout_rf_quality_t;

typedef struct {
    uint16_t mhz, width_khz;
    uint16_t contrast;
    uint8_t kind;
} scout_rf_hit_t;

typedef struct {
    scout_rf_hit_t best, shown;
    uint16_t tracking_mhz;
    uint8_t seen, family_checks, family, missed;
} scout_rf_track_t;

#ifdef SCOUT_RUST_CORE
extern void scout_rf_shape(const int16_t *, scout_rf_shape_t *);
extern uint32_t scout_rf_timing(const uint32_t *, size_t, uint32_t, scout_rf_quality_t *);
extern unsigned scout_elrs_chirps(const uint32_t *, size_t, const int16_t *, scout_rf_quality_t *);
#endif

static inline bool scout_rf_near(unsigned a, unsigned b) {
    return a > b ? a - b <= 5 : b - a <= 5;
}

static inline bool scout_rf_finish_sweep(scout_rf_track_t *track) {
    const scout_rf_hit_t hit = track->best, before = track->shown;
    track->best = (scout_rf_hit_t){0};
    if (!hit.kind || !scout_rf_near(hit.mhz, track->tracking_mhz)) {
        track->tracking_mhz = hit.mhz;
        track->seen = track->family_checks = 0;
        track->family = SCOUT_RF_NONE;
    }
    if (hit.kind) {
        if (track->seen < 3) track->seen++;
        if (hit.kind == SCOUT_RF_UNKNOWN) {
            if (track->family_checks) track->family_checks--;
        } else {
            if (track->family != hit.kind) track->family_checks = 0;
            track->family = hit.kind;
            if (track->family_checks < 3) track->family_checks++;
        }
    }
    if (track->seen >= 2) {
        const bool same = before.kind && scout_rf_near(hit.mhz, before.mhz);
        const bool named = track->family_checks >= 3 ||
                           (same && before.kind == track->family && track->family_checks == 2);
        if (!same) track->shown = hit;
        track->shown.kind = named ? track->family : SCOUT_RF_UNKNOWN;
        track->missed = 0;
    } else if (track->shown.kind && ++track->missed >= 2) {
        track->shown = (scout_rf_hit_t){0};
        track->missed = 0;
    }
    return track->shown.kind != before.kind || track->shown.mhz != before.mhz;
}
