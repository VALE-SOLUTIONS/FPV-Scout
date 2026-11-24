/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../main/families/c5_c6_c61/composite/patterns.h"
#include <assert.h>
#include <stdio.h>

static uint8_t dma[640 * 312 + 2];

static unsigned sample(const cvbs_frame_t *frame, unsigned line, unsigned x)
{
    size_t offset = (size_t)line * frame->samples + x;
    if (frame->bus_width == 8) return frame->dma[offset];
    return (frame->dma[offset / 4] >> (2 * (offset % 4))) & 3u;
}

static void verify_timing(const cvbs_frame_t *frame)
{
    for (unsigned line = 0; line < frame->lines; line++) {
        unsigned half = frame->samples / 2;
        for (unsigned x = 0; x < frame->samples; x++) {
            bool active = line >= frame->first_row && line < frame->first_row + CVBS_H &&
                          x >= frame->active_x && x < frame->active_x + 2 * CVBS_W;
            if (active) continue;
            unsigned pulses = frame->mode == CVBS_PAL ? 5u : 6u;
            unsigned interval = (line * 2 + x / half) / pulses;
            bool sync = interval == 0 || interval == 2 ? x % half < 23 :
                        interval == 1 ? x % half < half - 47 : x < 47;
            assert(sample(frame, line, x) == (sync ? 0u : (unsigned)frame->blank));
        }
    }
}

static void check(cvbs_mode_t mode, unsigned width)
{
    cvbs_frame_t frame;
    cvbs_configure(&frame, dma + 1, mode, width);
    size_t bytes = cvbs_bytes(&frame);
    assert(bytes == (mode == CVBS_PAL ? 199680u : 166632u) / (width == 2 ? 4u : 1u));
    dma[0] = dma[bytes + 1] = 0xa5;
    cvbs_sync(&frame);
    pattern_draw(&frame, PAT_BLACK, 0);
    for (unsigned y = 0; y < CVBS_H; y++) {
        for (unsigned x = 0; x < 2 * CVBS_W; x++) {
            assert(sample(&frame, frame.first_row + y, frame.active_x + x) == frame.black);
        }
    }
    verify_timing(&frame);
    pattern_draw(&frame, PAT_RAMP, 0);
    unsigned seen[256] = {0};
    unsigned distinct = 0;
    for (unsigned x = 0; x < CVBS_W; x++) {
        unsigned luma = cvbs_luma_at(&frame, x, 0);
        if (x) assert(luma >= cvbs_luma_at(&frame, x - 1, 0));
        if (!seen[luma]++) distinct++;
    }
    assert(distinct == (width == 2 ? 3u : 16u));
    assert(cvbs_luma_at(&frame, 0, 0) == 0 && cvbs_luma_at(&frame, 255, 0) == 255);
    assert(!cvbs_set_levels(&frame, 0, frame.max_code));
    assert(!cvbs_set_levels(&frame, frame.blank, frame.max_code + 1u));
    assert(cvbs_set_levels(&frame, frame.blank, frame.max_code));
    pattern_draw(&frame, PAT_LEVELS, 0);
    assert(sample(&frame, frame.first_row, frame.active_x + 255) == frame.black);
    assert(sample(&frame, frame.first_row, frame.active_x + 256) == frame.white);
    pattern_draw(&frame, PAT_BOUNCE, 0);
    assert(cvbs_luma_at(&frame, 0, 0) == 255);
    pattern_draw(&frame, PAT_BOUNCE, 12);
    assert(cvbs_luma_at(&frame, 0, 0) == 0 && cvbs_luma_at(&frame, 36, 24) == 255);
    verify_timing(&frame);
    assert(dma[0] == 0xa5 && dma[bytes + 1] == 0xa5);
}

int main(void)
{
    check(CVBS_NTSC, 2);
    check(CVBS_NTSC, 8);
    check(CVBS_PAL, 2);
    check(CVBS_PAL, 8);
    puts("PASS: both formats and DAC packings, blank privacy pattern, sync/porches, levels, animation and bounds");
}
