/* SPDX-License-Identifier: GPL-3.0-only
 * Composite raster adapted from C5VTX. */
#include "cvbs.h"

void cvbs_configure(cvbs_frame_t *frame, uint8_t *dma, cvbs_mode_t mode, unsigned bus_width)
{
    bool pal = mode == CVBS_PAL;
    bool packed = bus_width == 2;
    *frame = (cvbs_frame_t){
        .dma = dma, .samples = pal ? 640 : 636, .lines = pal ? 312 : 262,
        .first_row = pal ? 47 : 21, .active_x = pal ? 104 : 100,
        .bus_width = bus_width, .blank = packed ? 1 : 18,
        .black = packed ? 1 : (pal ? 18 : 21), .white = packed ? 3 : 62,
        .max_code = packed ? 3 : 63, .mode = mode,
    };
}

size_t cvbs_bytes(const cvbs_frame_t *frame)
{
    return (size_t)frame->lines * frame->samples * frame->bus_width / 8;
}

static uint8_t timing_code(const cvbs_frame_t *frame, unsigned line, unsigned column)
{
    /* Progressive raster, with PAL's five or NTSC's six pulses per vertical interval. */
    unsigned half = frame->samples / 2;
    unsigned pulse = line * 2 + (column >= half);
    unsigned count = frame->mode == CVBS_PAL ? 5u : 6u;
    if (pulse < 3 * count) {
        unsigned low = pulse >= count && pulse < 2 * count ? half - 47 : 23;
        return column % half < low ? 0 : frame->blank;
    }
    return column < 47 ? 0 : frame->blank;
}

void cvbs_sync(cvbs_frame_t *frame)
{
    for (unsigned line = 0; line < frame->lines; line++) {
        if (frame->bus_width == 2) {
            uint8_t *row = frame->dma + (size_t)line * frame->samples / 4;
            for (unsigned x = 0; x < frame->samples; x += 4) {
                unsigned packed = 0;
                for (unsigned i = 0; i < 4; i++) packed |= timing_code(frame, line, x + i) << (2 * i);
                row[x / 4] = (uint8_t)packed;
            }
        } else {
            uint8_t *row = frame->dma + (size_t)line * frame->samples;
            for (unsigned x = 0; x < frame->samples; x++) row[x] = timing_code(frame, line, x);
        }
    }
}

static unsigned dac_code(const cvbs_frame_t *frame, uint8_t luma)
{
    return frame->black + (luma * (frame->white - frame->black) + 127u) / 255u;
}

void cvbs_draw_row(cvbs_frame_t *frame, unsigned y, const uint8_t luma[CVBS_W])
{
    if (y >= CVBS_H) return;
    size_t first = (size_t)(frame->first_row + y) * frame->samples + frame->active_x;
    if (frame->bus_width == 2) {
        uint8_t *row = frame->dma + first / 4;
        for (unsigned x = 0; x < CVBS_W; x += 2) {
            unsigned a = dac_code(frame, luma[x]);
            unsigned b = dac_code(frame, luma[x + 1]);
            row[x / 2] = (uint8_t)(a * 5u | b * 80u);
        }
    } else {
        uint8_t *row = frame->dma + first;
        for (unsigned x = 0; x < CVBS_W; x++) {
            row[2 * x] = row[2 * x + 1] = (uint8_t)dac_code(frame, luma[x]);
        }
    }
}

uint8_t cvbs_luma_at(const cvbs_frame_t *frame, unsigned x, unsigned y)
{
    if (x >= CVBS_W || y >= CVBS_H) return 0;
    size_t sample = (size_t)(frame->first_row + y) * frame->samples + frame->active_x + 2 * x;
    unsigned code = frame->bus_width == 2 ? (frame->dma[sample / 4] >> (2 * (sample % 4))) & 3u
                                        : frame->dma[sample];
    if (code <= frame->black) return 0;
    if (code >= frame->white) return 255;
    return (uint8_t)((code - frame->black) * 255u / (frame->white - frame->black));
}

bool cvbs_set_levels(cvbs_frame_t *frame, unsigned black, unsigned white)
{
    if (black < frame->blank || white <= black || white > frame->max_code) return false;
    frame->black = (uint8_t)black;
    frame->white = (uint8_t)white;
    return true;
}
