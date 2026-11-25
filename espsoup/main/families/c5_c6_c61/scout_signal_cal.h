/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { SCOUT_CAL_MENU, SCOUT_CAL_ANALOG, SCOUT_CAL_DIGITAL, SCOUT_CAL_CONTROL, SCOUT_CAL_RID };
enum { SCOUT_CAL_SETUP, SCOUT_CAL_BASELINE, SCOUT_CAL_READY, SCOUT_CAL_CHECK,
       SCOUT_CAL_REVIEW, SCOUT_CAL_SAVED, SCOUT_CAL_FAILED };

typedef struct {
    int16_t floor, level, other_level;
    uint16_t mhz;
    uint8_t signal, quality;
} scout_cal_reference_t;

typedef struct {
    uint8_t tool, stage, baseline_passes;
    bool baseline_ready, baseline_error, saved_valid, save_failed;
    int32_t baseline_total;
    int64_t deadline, next_sample;
    int16_t *quiet_plot;
    scout_cal_reference_t result, saved;
} scout_signal_cal_t;

static inline bool scout_cal_peak_rise(const int16_t *quiet, const int16_t *active, int first, int last) {
    if (!quiet) return false;
    int before = INT16_MIN, after = INT16_MIN;
    for (int i = first; i <= last; ++i) {
        if (quiet[i] > before) before = quiet[i];
        if (active[i] > after) after = active[i];
    }
    return before != INT16_MIN && after != INT16_MIN && after - before >= 600;
}
