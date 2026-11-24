/* SPDX-License-Identifier: GPL-3.0-only
 * Test patterns and digits adapted from C5VTX. */
#include "patterns.h"

const char *const pattern_names[PAT_COUNT] = {"black", "bars", "grid", "ramp", "levels", "bounce"};

static const uint8_t digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
};

static unsigned reflect(unsigned phase, unsigned extent)
{
    phase %= 2 * extent;
    return phase < extent ? phase : 2 * extent - phase;
}

void pattern_draw(cvbs_frame_t *frame, pattern_t pattern, unsigned tick)
{
    uint8_t row[CVBS_W];
    unsigned bx = reflect(tick * 3u, CVBS_W - 24u);
    unsigned by = reflect(tick * 2u, CVBS_H - 48u);
    unsigned value = tick % 1000;
    for (unsigned y = 0; y < CVBS_H; y++) {
        for (unsigned x = 0; x < CVBS_W; x++) {
            unsigned gray = 0;
            switch (pattern) {
            case PAT_BARS: gray = 255u - (x / 32u) * 255u / 7u; break;
            case PAT_GRID: gray = x % 32 == 0 || y % 24 == 0 ? 255 : 0; break;
            case PAT_RAMP: gray = (x / 16u) * 17u; break;
            case PAT_LEVELS: gray = x < CVBS_W / 2 ? 0 : 255; break;
            case PAT_BOUNCE:
                gray = x >= bx && x < bx + 24 && y >= by && y < by + 24 ? 255 : 0;
                if (y >= 220 && y < 234 && x >= 16 && x < 52) {
                    unsigned slot = (x - 16) / 12;
                    unsigned column = ((x - 16) % 12) / 2;
                    unsigned digit = slot == 0 ? value / 100 : slot == 1 ? value / 10 % 10 : value % 10;
                    if (column < 5 && (digits[digit][(y - 220) / 2] & (16u >> column))) gray = 255;
                }
                break;
            default: break;
            }
            row[x] = (uint8_t)gray;
        }
        cvbs_draw_row(frame, y, row);
    }
}
