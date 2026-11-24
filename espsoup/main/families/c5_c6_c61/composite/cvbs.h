/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { CVBS_CLOCK = 10000000, CVBS_W = 256, CVBS_H = 240 };
typedef enum { CVBS_NTSC, CVBS_PAL } cvbs_mode_t;

typedef struct {
    uint8_t *dma;
    unsigned samples, lines, first_row, active_x, bus_width;
    uint8_t blank, black, white, max_code;
    cvbs_mode_t mode;
} cvbs_frame_t;

void cvbs_configure(cvbs_frame_t *frame, uint8_t *dma, cvbs_mode_t mode, unsigned bus_width);
size_t cvbs_bytes(const cvbs_frame_t *frame);
void cvbs_sync(cvbs_frame_t *frame);
void cvbs_draw_row(cvbs_frame_t *frame, unsigned y, const uint8_t luma[CVBS_W]);
uint8_t cvbs_luma_at(const cvbs_frame_t *frame, unsigned x, unsigned y);
bool cvbs_set_levels(cvbs_frame_t *frame, unsigned black, unsigned white);
