/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "esp_err.h"
#include "patterns.h"

typedef struct {
    cvbs_frame_t frame;
    pattern_t pattern;
    esp_err_t error;
    bool running;
} scout_bench_view_t;

extern scout_bench_view_t scout_bench;
void scout_bench_open(void);
bool scout_bench_close(void);
void scout_bench_select(pattern_t pattern);
void scout_bench_next(void);
void scout_bench_format(cvbs_mode_t mode);
bool scout_bench_levels(unsigned black, unsigned white);
void scout_bench_step(int64_t now);
