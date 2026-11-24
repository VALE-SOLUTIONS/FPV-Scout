/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "cvbs.h"

typedef enum { PAT_BLACK, PAT_BARS, PAT_GRID, PAT_RAMP, PAT_LEVELS, PAT_BOUNCE, PAT_COUNT } pattern_t;
extern const char *const pattern_names[PAT_COUNT];
void pattern_draw(cvbs_frame_t *frame, pattern_t pattern, unsigned tick);
