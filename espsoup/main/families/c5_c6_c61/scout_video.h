/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from Claude's NM-CYD-C5 main/cyd_view.c port of C5VRX by Twotoz
 * and the C5VRX contributors, upstream 8b60fef0f43f43a5305d941568c5975386867fc0.
 * https://github.com/Twotoz/C5VRX - https://twotoz.github.io/C5VRX/
 * The official website provides the original project and Discord invite.
 * Independent derivative, not an official or endorsed C5VRX release.
 * Changes: finite ESPsoup IQ10 snapshots, fixed completed-dump workspace,
 * and a buffered LCD viewport. No PARLIO stream, DAC, or app switch.
 * Snapshots have real gaps; this is a progressively filled grayscale preview.
 */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#endif
#include "scout_layout.h"
#define sv_RATE      20            /* samples per microsecond */
#define sv_SNAP      16380u         /* 819 us at 20 MS/s */
#define sv_PAL_LINE16   (1280 * 16)
#define sv_NTSC_LINE16  20338      /* 1271.111 samples x16 */
#define sv_FRAME_HOLD_US 3000000
#define sv_FIELD_ACQUIRE_US 400000
#define sv_FIELD_TRACK_US 1250000
#define sv_MAX_INTERPOLATED_GAP 6
#define sv_PREFERRED_LINE_HOLD_MS 750
#define sv_PARTIAL_PREVIEW_WAIT_US 2000000
#define sv_DISPLAY_INTERVAL_US 16667
#define sv_SPARSE_DISPLAY_INTERVAL_US 66000
#define sv_U2N       (sv_SNAP / 2 + 1)                   /* unwrapped phase every 2nd sample */
#define sv_LSTEP     (sv_RATE / 2)                    /* 0.5 us grid */
#define sv_LWIN      sv_RATE                          /* 1 us window */
#define sv_LN        ((sv_SNAP - 2 - sv_LWIN) / sv_LSTEP)      /* sv_level points */
#define sv_MAXP      48                               /* pulses per snapshot */

enum { sv_P_EQ, sv_P_H, sv_P_BROAD };
typedef struct { int32_t edge; int16_t offset256; uint8_t cls; } sv_pulse_t;
static int32_t *sv_s_u2;                              /* unwrapped phase at even samples */
static int32_t *sv_s_lv;                           /* 1 us sv_levels on a 0.5 us grid, sync = low */
static sv_pulse_t *sv_s_p;
static uint8_t *sv_picture;
static unsigned sv_s_np;
static unsigned sv_probe_frames;
#ifdef SCOUT_VIDEO_PROBE
static struct {
    int64_t started, capture_us, decode_us, render_us, last_render, longest_gap;
    int64_t phase_us, rows_us;
    unsigned captures, frames, lcd_rows, decoded_rows;
} sv_perf;
#endif

static struct {
    int pol;                                       /* +1 / -1: sync is the low extreme of pol*freq */
    unsigned pol_miss;
    bool lv_ok;
    int32_t S16, B16;                              /* sync tip / blanking sv_level x16 */
    bool pal;
    unsigned format_votes;
    int32_t hper16;                                /* measured line period x16 (samples) */
    int32_t hsync16;                               /* line period from horizontal pulses alone */
    unsigned hsync_reads;
    unsigned half_lines;                          /* measured half-lines per video field */
    bool tv_ok, field_timed;
    int64_t tv;                                    /* field reference: first broad pulse, sample ticks */
    int32_t field256;                              /* tracked period in 1/256-sample units */
    int64_t last_sync_us, first_row_us, last_row_us, last_render_us, next_render_us;
    unsigned rows, last_render_rows;
    uint8_t row_seen[SCOUT_MAX_VIDEO_H / 8 + 1];
    uint8_t row_dirty[SCOUT_MAX_VIDEO_H / 8 + 1];
    char overlay[64];
    uint32_t preferred_ms[SCOUT_MAX_VIDEO_H];
} sv_v = { .pol = 1, .hper16 = sv_NTSC_LINE16 };

/* round(atan(n / 128) * 1024 / (2 * pi)) for the first octant. */
#ifdef ESP_PLATFORM
DRAM_ATTR
#endif
static const uint8_t sv_octant[129] = {
    0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 13, 14, 15, 16, 18, 19,
    20, 22, 23, 24, 25, 27, 28, 29, 30, 31, 33, 34, 35, 36, 38, 39,
    40, 41, 42, 44, 45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56, 57,
    58, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 74, 75,
    76, 77, 78, 79, 80, 81, 82, 83, 84, 84, 85, 86, 87, 88, 89, 90,
    91, 92, 93, 94, 95, 96, 96, 97, 98, 99, 100, 101, 102, 102, 103, 104,
    105, 106, 106, 107, 108, 109, 110, 110, 111, 112, 113, 113, 114, 115, 116, 116,
    117, 118, 119, 119, 120, 121, 121, 122, 123, 123, 124, 125, 125, 126, 127, 127,
    128
};

#ifdef ESP_PLATFORM
__attribute__((always_inline))
#endif
static inline unsigned sv_iq_phase(uint32_t word) {
    const int32_t i = ((int32_t)(word << 22)) >> 22;
    const int32_t q = ((int32_t)(word << 12)) >> 22;
    const unsigned x = abs(i), y = abs(q);
    if (!(x | y)) return 0;
    unsigned a = y <= x ? sv_octant[(y * 128 + x / 2) / x]
                        : 256 - sv_octant[(x * 128 + y / 2) / y];
    if (i < 0) a = 512 - a;
    return q < 0 ? (1024 - a) & 1023 : a;
}

/* Keep the raw IQ precision through unwrapping, then store in level units. */
#ifdef ESP_PLATFORM
IRAM_ATTR
#endif
static void sv_discriminate(const uint32_t *samples) {
    int32_t u = 0;
    unsigned prev = sv_iq_phase(samples[0]);
    sv_s_u2[0] = 0;
    for (unsigned n = 2; n < sv_SNAP; n += 2) {
        const unsigned middle = sv_iq_phase(samples[n - 1]);
        const unsigned phase = sv_iq_phase(samples[n]);
        u += (int)((middle - prev + 512u) & 1023u) + (int)((phase - middle + 512u) & 1023u) - 1024;
        prev = phase;
        sv_s_u2[n / 2] = (u + 2) >> 2;
    }
}

/* Mean frequency over [a, b) (even sample indices), scaled to 1 us. */
static inline int32_t sv_level(int32_t a, int32_t b)
{
    return sv_v.pol * (sv_s_u2[b >> 1] - sv_s_u2[a >> 1]) * (sv_LWIN / 2) / ((b - a) >> 1);
}
static void sv_levels(void)
{
    for (unsigned k = 0; k < sv_LN; ++k) sv_s_lv[k] = sv_level(k * sv_LSTEP, k * sv_LSTEP + sv_LWIN);
}

/* Runs below th: H sync 3-6.5 us, equalising 1-2.5 us, broad 23-31 us. */
static void sv_find_pulses(int32_t th)
{
    sv_s_np = 0;
    int k0 = -1;
    for (int k = 0; k <= (int)sv_LN && sv_s_np < sv_MAXP; ++k) {
        const bool low = k < (int)sv_LN && sv_s_lv[k] < th;
        if (low && k0 < 0) k0 = k;
        if (!low && k0 >= 0) {
            const int d = k - k0;
            if (k0 > 0 && k < (int)sv_LN) {
                sv_pulse_t *p = &sv_s_p[sv_s_np];
                p->cls = d >= 46 && d <= 62 ? sv_P_BROAD : d >= 6 && d <= 13 ? sv_P_H : d >= 2 && d <= 5 ? sv_P_EQ : 0xFF;
                if (p->cls != 0xFF) {
                    /* Interpolate the threshold crossing within the final 100 ns step. */
                    int32_t e = k0 * sv_LSTEP + sv_LSTEP;
                    p->offset256 = 0;
                    for (int32_t s = (k0 - 1) * sv_LSTEP + sv_LSTEP; s <= k0 * sv_LSTEP + sv_LWIN; s += 2) {
                        if (s - sv_LWIN / 2 < 0 || s + sv_LWIN / 2 > sv_SNAP - 2) continue;
                        const int32_t current = sv_level(s - sv_LWIN / 2, s + sv_LWIN / 2);
                        if (current >= th) continue;
                        e = s;
                        if (s >= sv_LWIN / 2 + 2) {
                            const int32_t previous = sv_level(s - sv_LWIN / 2 - 2, s + sv_LWIN / 2 - 2);
                            if (previous >= th)
                                p->offset256 = -(int16_t)((th - current) * 512 / (previous - current));
                        }
                        break;
                    }
                    p->edge = e;
                    ++sv_s_np;
                }
            }
            k0 = -1;
        }
    }
}

static int32_t sv_acquire_threshold(void)
{
    int32_t lo = INT32_MAX, hi = INT32_MIN;
    unsigned hist[64] = {0};
    for (unsigned k = 0; k < sv_LN; ++k) {
        if (sv_s_lv[k] < lo) lo = sv_s_lv[k];
        if (sv_s_lv[k] > hi) hi = sv_s_lv[k];
    }
    if (lo == hi) return lo;
    for (unsigned k = 0; k < sv_LN; ++k)
        hist[(sv_s_lv[k] - lo) * 63 / (hi - lo)]++;
    unsigned total = 0, q03 = 0, q50 = 0;
    bool got_tip = false;
    for (unsigned k = 0; k < 64; ++k) {
        total += hist[k];
        if (!got_tip && total >= (sv_LN * 3 + 99) / 100) { q03 = k; got_tip = true; }
        if (total >= sv_LN / 2) { q50 = k; break; }
    }
    /* A vertical snapshot may be mostly sync. Keep a threshold above its tip. */
    if (q50 <= q03 + 1) return lo + (hi - lo) * 15 / 100;
    return lo + (hi - lo) * (3 * (int32_t)q03 + 2 * (int32_t)q50) / (5 * 63);
}

static bool sv_h_pair(const sv_pulse_t *a, const sv_pulse_t *b)
{
    const int32_t d = b->edge - a->edge;
    return a->cls == sv_P_H && b->cls == sv_P_H && d >= 1250 && d <= 1300;
}

/* Isolated pulse pairs can be picture edges or noise. */
static bool sv_h_chain(unsigned j) {
    const bool prev = j && sv_h_pair(&sv_s_p[j - 1], &sv_s_p[j]);
    const bool next = j + 1 < sv_s_np && sv_h_pair(&sv_s_p[j], &sv_s_p[j + 1]);
    return (prev && next) ||
           (prev && j >= 2 && sv_h_pair(&sv_s_p[j - 2], &sv_s_p[j - 1])) ||
           (next && j + 2 < sv_s_np && sv_h_pair(&sv_s_p[j + 1], &sv_s_p[j + 2]));
}

/* Choose the polarity whose short low runs repeat at the line period. */
static int sv_sync_score(void)
{
    sv_find_pulses(sv_acquire_threshold());
    int score = 0;
    for (unsigned j = 1; j < sv_s_np; ++j)
        score += sv_h_pair(&sv_s_p[j - 1], &sv_s_p[j]);
    return score;
}
static void sv_choose_polarity(void)
{
    const int previous = sv_v.pol;
    sv_v.pol = 1; sv_levels(); const int a = sv_sync_score();
    sv_v.pol = -1; sv_levels(); const int b = sv_sync_score();
    sv_v.pol = b > a ? -1 : a > b ? 1 : previous;
    sv_levels();
    sv_v.lv_ok = false;
}

/* ------------------------------------------------------------------ drawing */
static inline uint16_t sv_grey565(int g)
{
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    const uint16_t c = (uint16_t)(((g & 0xF8) << 8) | ((g & 0xFC) << 3) | (g >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

static inline int32_t sv_phase_at(int32_t sample256) {
    const int32_t index = sample256 / 512;
    const int32_t fraction = sample256 % 512;
    return sv_s_u2[index] * 16 + (sv_s_u2[index + 1] - sv_s_u2[index]) * fraction / 32;
}

static inline bool sv_phase_outside(int32_t index, int32_t low, int32_t high) {
    const int32_t level = sv_v.pol * (sv_s_u2[index + 1] - sv_s_u2[index]) * (sv_LWIN / 2);
    return level < low || level > high;
}

static inline bool sv_phase_span_clear(int32_t first, int32_t end, int32_t low, int32_t high) {
    for (int32_t i = first; i < end; ++i)
        if (sv_phase_outside(i, low, high)) return false;
    return true;
}

static void sv_draw_line(int32_t e, int32_t edge_offset256, int row, int32_t porch, int32_t depth, bool core_clean)
{
    const int32_t a0 = e + (sv_v.pal ? 210 : 188);    /* active video: 10.5 us PAL, 9.4 us NTSC after 0H */
    const int32_t w = sv_v.pal ? 1040 : 1052;         /* 52.0 / 52.6 us */
    const int32_t S = sv_v.S16 / 16, B = sv_v.B16 / 16;
    int32_t span = (B - S) * (sv_v.pal ? 233 : 250) / 100;   /* blanking -> white: 100 / 43 or 100 / 40 of the sync depth */
    if (span < 4) span = 4;
    const int width = SCOUT_VIDEO_W;
    const int32_t step = (w * 256 + width / 2) / width;
    const int32_t low = porch - 3 * depth / 2, high = porch + 3 * depth;
    uint16_t *px = cyd_band();
    uint8_t *line = sv_picture ? sv_picture + row * width : NULL;
    const unsigned mask = 1u << (row % 8);
    const bool known_row = line && (sv_v.row_seen[row / 8] & mask);
    bool changed = !known_row;
    int32_t offset = a0 * 256 + edge_offset256, phase = sv_phase_at(offset);
    /* Fractional alignment can extend past the nominal span checked for noise. */
    const bool mask_impulses = known_row && (!core_clean ||
        !sv_phase_span_clear(offset / 512, a0 / 2, low, high) ||
        !sv_phase_span_clear((a0 + w) / 2, (offset + width * step - 1) / 512 + 1, low, high));
    for (int x = 0; x < width; ++x) {
        const int32_t next = offset + step, end_phase = sv_phase_at(next);
        const int32_t level = sv_v.pol * (end_phase - phase) * (16 * sv_RATE) / step;
        const bool reliable = !mask_impulses || sv_phase_span_clear(offset / 512, (next - 1) / 512 + 1, low, high);
        offset = next;
        phase = end_phase;
        if (!reliable) continue;
        const int gray = (level - B) * 255 / span;
        if (line) {
            const int value = gray < 0 ? 0 : gray > 255 ? 255 : gray;
            const int previous = known_row ? line[x] : value;
            /* Average low-amplitude noise while letting sharp scene changes through. */
            const int output = known_row && abs(value - previous) <= 24 ? (previous + value + 1) / 2 : value;
            line[x] = output;
            changed |= (output >> 2) != (previous >> 2);
        } else px[x] = sv_grey565(gray);
    }
    if (!(sv_v.row_seen[row / 8] & mask)) {
        sv_v.row_seen[row / 8] |= mask;
        sv_v.rows++;
    }
    if (changed) sv_v.row_dirty[row / 8] |= mask;
    if (!line)
        cyd_blit(SCOUT_VIDEO_X, SCOUT_VIDEO_Y + row, width, 1, px);
}

static int sv_next_row(int row, int height) {
    while (row < height && !(sv_v.row_seen[row / 8] & (1u << (row % 8)))) row++;
    return row;
}

static bool sv_display_row_dirty(int row, int source, bool overlay_changed) {
    return (sv_v.row_dirty[source / 8] & (1u << (source % 8))) || (overlay_changed && row < 20);
}

static bool sv_render_viewport(bool readback, const char *overlay, bool overlay_changed) {
    const int width = SCOUT_VIDEO_W, height = SCOUT_VIDEO_H;
    const int out_height = overlay && cyd_landscape() ? cyd_height() : height;
    const int origin_y = overlay ? (cyd_height() - out_height) / 2 : SCOUT_VIDEO_Y;
    /* Until every source row is present, newly decoded rows can change interpolation. */
    const bool full = readback || sv_v.rows != (unsigned)height || sv_v.last_render_rows != (unsigned)height;
    bool rendered = false;
    int upper = sv_next_row(0, height);
    if (upper == height) return false;
    int lower = upper;
    uint16_t *pixels = cyd_band();
    for (int y = 0; y < out_height; ) {
        int rows = out_height - y < CYD_BAND_LINES ? out_height - y : CYD_BAND_LINES;
        if (!full) {
            if (!sv_display_row_dirty(y,overlay ? y * height / out_height : y,overlay_changed)) { ++y; continue; }
            const int end = y + rows;
            int next = y + 1;
            while (next < end && sv_display_row_dirty(next,overlay ? next * height / out_height : next,overlay_changed)) ++next;
            rows = next - y;
        }
        if (!overlay && sv_v.rows == (unsigned)height) {
            const uint8_t *gray = sv_picture + y * width;
            for (int x = 0; x < rows * width; x++) pixels[x] = sv_grey565(gray[x]);
        } else for (int r = 0; r < rows; r++) {
            const int row = overlay ? (y + r) * height / out_height : y + r;
            uint16_t *out = pixels + r * width;
            if (sv_v.rows != (unsigned)height && row > upper) {
                lower = upper;
                upper = sv_next_row(upper + 1, height);
            }
            const int gap = upper - lower;
            if (sv_v.rows != (unsigned)height && row > lower && row < upper && upper < height && gap <= sv_MAX_INTERPOLATED_GAP) {
                const uint8_t *above = sv_picture + lower * width, *below = sv_picture + upper * width;
                const int span = row - lower;
                for (int x = 0; x < width; x++)
                    out[x] = sv_grey565((above[x] * (gap - span) + below[x] * span + gap / 2) / gap);
            } else {
                int source = row;
                if (sv_v.rows != (unsigned)height)
                    source = row <= lower || upper == height || row - lower <= upper - row ? lower : upper;
                const uint8_t *gray = sv_picture + source * width;
                for (int x = 0; x < width; x++) out[x] = sv_grey565(gray[x]);
            }
            if (overlay && y+r < 20) {
                cyd_label_overlay_row(out,0,y+r,width,7,5,overlay,CYD_BLACK,CYD_FONT_SMALL);
                cyd_label_overlay_row(out,0,y+r,width,6,4,overlay,CYD_WHITE,CYD_FONT_SMALL);
            }
        }
#ifdef SCOUT_VIDEO_PROBE
        if (readback) {
            if (!send_bytes(pixels, width * rows * sizeof(*pixels))) return false;
        } else {
            sv_perf.lcd_rows += rows;
#endif
            cyd_blit(SCOUT_VIDEO_X, origin_y + y, width, rows, pixels);
#ifdef SCOUT_VIDEO_PROBE
        }
#endif
        rendered = true;
        y += rows;
    }
    if (!readback) {
        sv_v.last_render_rows = sv_v.rows;
        memset(sv_v.row_dirty, 0, sizeof(sv_v.row_dirty));
    }
    return rendered;
}

static void scout_video_invalidate(void) {
    sv_v.last_render_rows = 0;
    sv_v.last_render_us = sv_v.next_render_us = 0;
    sv_v.overlay[0] = '\0';
}

static void scout_video_render_mode(int64_t now, const char *overlay) {
    if (!sv_picture || !sv_v.rows || now < sv_v.next_render_us) return;
    const bool changed = overlay && strcmp(sv_v.overlay,overlay);
    if (!changed && sv_v.last_row_us <= sv_v.last_render_us) return;
#ifdef SCOUT_VIDEO_PROBE
    const int64_t begin = esp_timer_get_time();
#endif
    if (!sv_render_viewport(false,overlay,changed)) return;
    if (changed) snprintf(sv_v.overlay,sizeof(sv_v.overlay),"%s",overlay);
    sv_v.last_render_us = now;
    const int64_t interval = sv_v.rows == (unsigned)SCOUT_VIDEO_H ? sv_DISPLAY_INTERVAL_US : sv_SPARSE_DISPLAY_INTERVAL_US;
    /* Keep the cadence after a late refresh without making up missed frames. */
    if (!sv_v.next_render_us || now - sv_v.next_render_us >= interval) sv_v.next_render_us = now;
    sv_v.next_render_us += interval;
#ifdef SCOUT_VIDEO_PROBE
    sv_perf.render_us += esp_timer_get_time() - begin;
    if (sv_perf.last_render && begin - sv_perf.last_render > sv_perf.longest_gap)
        sv_perf.longest_gap = begin - sv_perf.last_render;
    sv_perf.last_render = begin;
    sv_perf.frames++;
#endif
}

static void scout_video_render(int64_t now) { scout_video_render_mode(now,NULL); }

/* Camera and test-pattern sources can use integer-line progressive fields. */
static unsigned sv_half_lines(void) {
    return sv_v.half_lines ? sv_v.half_lines : sv_v.pal ? 625 : 525;
}

/* Fit the field clock independently of the line count. */
static void sv_field_reference(int64_t ticks)
{
    const int32_t nominal_line16 = sv_v.pal ? sv_PAL_LINE16 : sv_NTSC_LINE16;
    const int nominal_half_lines = sv_v.pal ? 625 : 525;
    const int64_t period256 = sv_v.field_timed ? sv_v.field256 : (int64_t)sv_v.hper16 * 8 * sv_half_lines();
    if (sv_v.tv) {
        const int64_t elapsed256 = (ticks - sv_v.tv) * 256;
        /* Both ends of one serration train may identify the same field. */
        if (elapsed256 < period256 / 2) return;
        const int64_t fields = (elapsed256 + period256 / 2) / period256;
        if (fields <= 240) {
            const int32_t measured_field = (int32_t)((elapsed256 + fields / 2) / fields);
            unsigned half_lines = sv_half_lines();
            if (!sv_v.half_lines && sv_v.hsync_reads >= 4) {
                const int32_t half_line256 = sv_v.hsync16 * 8;
                const int guess = (measured_field + half_line256 / 2) / half_line256;
                if (abs(guess - nominal_half_lines) <= 1) half_lines = (unsigned)guess;
            }
            const int32_t measured = (measured_field + 4 * half_lines) / (8 * half_lines);
            if (abs(measured - nominal_line16) <= nominal_line16 / 200) {
                sv_v.field256 = sv_v.field_timed ?
                    (3 * sv_v.field256 + measured_field + 2) / 4 : measured_field;
                if (sv_v.hsync_reads >= 4) sv_v.half_lines = half_lines;
                sv_v.hper16 = (sv_v.field256 + 4 * half_lines) / (8 * half_lines);
                sv_v.field_timed = true;
            }
        }
    }
    sv_v.tv = ticks;
    sv_v.tv_ok = true;
}

static int64_t sv_field_max_age(void) {
    return sv_v.field_timed ? sv_FIELD_TRACK_US : sv_FIELD_ACQUIRE_US;
}

/* Retain the previous row when a burst corrupts the active picture. */
enum { sv_LINE_REJECTED, sv_LINE_CLEAN, sv_LINE_IMPULSES };

static unsigned sv_active_video_quality(int32_t edge, int32_t porch, int32_t depth, bool pal) {
    const int32_t active = edge + (pal ? 210 : 188);
    const int32_t width = pal ? 1040 : 1052;
    if (active + width > sv_SNAP - 2) return sv_LINE_REJECTED;
    const int32_t low = porch - 3 * depth / 2, high = porch + 3 * depth;
    unsigned impulses = 0, bad_windows = 0;
    for (int32_t p = active; p < active + width; p += 2) {
        if (sv_phase_outside(p / 2, low, high) && ++impulses >= 8) return sv_LINE_REJECTED;
    }
    for (int32_t p = active; p + sv_LWIN <= active + width; p += sv_LWIN) {
        const int32_t level = sv_level(p, p + sv_LWIN);
        if ((level < porch - depth / 2 || level > high) && ++bad_windows >= 5) return sv_LINE_REJECTED;
    }
    return impulses ? sv_LINE_IMPULSES : sv_LINE_CLEAN;
}

static unsigned sv_horizontal_timing(int32_t *span) {
    unsigned pairs = 0;
    *span = 0;
    for (unsigned j = 1; j < sv_s_np; ++j) {
        if (!sv_h_pair(&sv_s_p[j - 1], &sv_s_p[j])) continue;
        *span += sv_s_p[j].edge - sv_s_p[j - 1].edge;
        pairs++;
    }
    return pairs;
}

/* Score the existing detector capture without touching the picture or the field clock. */
static unsigned scout_video_quality(int polarity) {
    const int previous = sv_v.pol;
    sv_v.pol = polarity;
    sv_discriminate(DUMP_BUF);
    sv_levels();
    sv_find_pulses(sv_acquire_threshold());
    int32_t span;
    const unsigned pairs = sv_horizontal_timing(&span);
    const bool pal = span * 2 > (int32_t)pairs * 2551;
    unsigned clean = 0;
    if (pairs >= 3) {
        for (unsigned j = 0; j < sv_s_np; ++j) {
            const int32_t edge = sv_s_p[j].edge;
            if (edge + 170 > sv_SNAP - 2 || !sv_h_chain(j)) continue;
            const int32_t porch = sv_level(edge + 120, edge + 170);
            const int32_t depth = porch - sv_level(edge + 20, edge + 80);
            if (depth > 0 && sv_active_video_quality(edge, porch, depth, pal) != sv_LINE_REJECTED) clean++;
        }
    }
    sv_v.pol = previous;
    return clean;
}

static void sv_clear_rows(void) {
    sv_v.rows = 0;
    sv_v.first_row_us = 0;
    sv_v.last_render_rows = 0;
    sv_v.next_render_us = 0;
    memset(sv_v.row_seen, 0, sizeof(sv_v.row_seen));
    memset(sv_v.preferred_ms, 0, sizeof(sv_v.preferred_ms));
}

/* A fixed source line keeps downsampled edges from alternating between two rows. */
static int sv_preferred_source_line(int row, int count, int height) {
    const int first = (row * count + height - 1) / height;
    const int centre = (2 * row + 1) * count / (2 * height);
    return centre < first ? first : centre;
}

static bool sv_fallback_source_due(int row, uint32_t now_ms) {
    const uint32_t last = sv_v.preferred_ms[row];
    return !last || now_ms - last >= sv_PREFERRED_LINE_HOLD_MS;
}

/* One noisy estimate must not replace a verified PAL/NTSC field clock. */
static bool sv_track_standard(bool pal, unsigned pairs) {
    if (pal == sv_v.pal) {
        sv_v.format_votes = 0;
        return true;
    }
    if (pairs < 6 || (sv_v.hsync_reads && ++sv_v.format_votes < 3)) return false;
    sv_v.pal = pal;
    sv_v.hper16 = pal ? sv_PAL_LINE16 : sv_NTSC_LINE16;
    sv_v.tv_ok = sv_v.field_timed = false;
    sv_v.hsync_reads = sv_v.half_lines = sv_v.format_votes = 0;
    sv_v.field256 = 0;
    sv_v.tv = 0;
    sv_v.last_row_us = 0;
    sv_clear_rows();
    return true;
}

/* Nearby horizontal sync pulses provide a cleaner subpixel edge. */
static int32_t sv_line_offset(unsigned j) {
    const sv_pulse_t *p = &sv_s_p[j];
    const int32_t period = (sv_v.hsync_reads ? sv_v.hsync16 : sv_v.hper16) * 16;
    int32_t offsets[5] = {p->offset256};
    unsigned count = 1;
    for (unsigned d = 1; d <= 2 && j >= d && sv_h_pair(&sv_s_p[j - d], &sv_s_p[j - d + 1]); ++d)
        offsets[count++] = (sv_s_p[j - d].edge - p->edge) * 256 + sv_s_p[j - d].offset256 + (int32_t)d * period;
    for (unsigned d = 1; d <= 2 && j + d < sv_s_np && sv_h_pair(&sv_s_p[j + d - 1], &sv_s_p[j + d]); ++d)
        offsets[count++] = (sv_s_p[j + d].edge - p->edge) * 256 + sv_s_p[j + d].offset256 - (int32_t)d * period;
    for (unsigned i = 1; i < count; ++i) {
        const int32_t offset = offsets[i];
        unsigned pos = i;
        while (pos && offsets[pos - 1] > offset) {
            offsets[pos] = offsets[pos - 1];
            --pos;
        }
        offsets[pos] = offset;
    }
    const int32_t median = (offsets[(count - 1) / 2] + offsets[count / 2]) / 2;
    return abs(median - p->offset256) <= 8 * 256 ? median : p->offset256;
}

static void sv_process_snapshot(int64_t boundary_us)
{
    const int64_t now = esp_timer_get_time(), tick_end = boundary_us * sv_RATE;
    sv_discriminate(DUMP_BUF);
#ifdef SCOUT_VIDEO_PROBE
    sv_perf.phase_us += esp_timer_get_time() - now;
#endif
    if (sv_v.tv_ok && now * sv_RATE - sv_v.tv > sv_RATE * sv_field_max_age())
        sv_v.tv_ok = false;
    if (sv_v.rows && now - sv_v.last_row_us >= sv_FRAME_HOLD_US) sv_clear_rows();
    /* Picture edges can look like sync in the wrong polarity. */
    if (!sv_v.tv_ok && (sv_probe_frames++ & 31u) == 0) sv_choose_polarity();
    sv_levels();
    sv_find_pulses(sv_v.lv_ok ? (sv_v.S16 + sv_v.B16) / 32 : sv_acquire_threshold());
    int32_t span;
    const unsigned pairs = sv_horizontal_timing(&span);
    if (pairs >= 3) {
        sv_v.pol_miss = 0;
        sv_v.last_sync_us = now;
        bool pal = span * 2 > (int32_t)pairs * 2551;
        if (!sv_track_standard(pal, pairs)) return;
        if (pairs >= 6) {
            const int32_t nominal = sv_v.pal ? sv_PAL_LINE16 : sv_NTSC_LINE16;
            const int32_t measured = (span * 16 + (int32_t)pairs / 2) / (int32_t)pairs;
            if (abs(measured - nominal) <= nominal / 200) {
                sv_v.hsync16 = sv_v.hsync_reads ? (7 * sv_v.hsync16 + measured + 4) / 8 : measured;
                if (sv_v.hsync_reads < 4) sv_v.hsync_reads++;
                if (!sv_v.field_timed) sv_v.hper16 = sv_v.hsync16;
            }
        }
    } else {
        sv_v.format_votes = 0;
        if (++sv_v.pol_miss >= 24) {
            sv_v.pol_miss = 0;
            sv_probe_frames = 0;
        }
    }
    const int64_t half = sv_v.hper16 / 32;

    for (unsigned j = 0; j < sv_s_np; ++j) {
        const sv_pulse_t *p = &sv_s_p[j];
        if (p->cls == sv_P_H) {
            /* Levels of this line: sync tip 1..4 us, back porch 6..8.5 us. */
            if (p->edge + 170 < sv_SNAP - 2 && sv_h_chain(j)) {
                const int32_t S = sv_level(p->edge + 20, p->edge + 80), B = sv_level(p->edge + 120, p->edge + 170);
                const int32_t depth = B - S, expected = (sv_v.B16 - sv_v.S16) / 16;
                if (depth > 0 && (!sv_v.lv_ok || (depth * 2 >= expected && depth <= expected * 2))) {
                    if (!sv_v.lv_ok) { sv_v.S16 = S * 16; sv_v.B16 = B * 16; sv_v.lv_ok = true; }
                    else { sv_v.S16 += S - sv_v.S16 / 16; sv_v.B16 += B - sv_v.B16 / 16; }
                }
            }
        } else if (p->cls == sv_P_BROAD) {
            /* First broad pulse of the field: an equalising pulse half a line before;
             * last one: an equalising pulse half a line after (5 PAL / 6 NTSC broad pulses). */
            const int64_t t = tick_end - (sv_SNAP - p->edge);
            if (j && sv_s_p[j - 1].cls == sv_P_EQ && llabs(p->edge - sv_s_p[j - 1].edge - half) < 40)
                sv_field_reference(t);
            else if (j + 1 < sv_s_np && sv_s_p[j + 1].cls == sv_P_EQ && llabs(sv_s_p[j + 1].edge - p->edge - half) < 40) {
                sv_field_reference(t - (int64_t)(sv_v.pal ? 4 : 5) * half);
            }
        }
    }
    /* A provisional field period misplaces rows on progressive sources. */
    if (!sv_v.tv_ok || !sv_v.field_timed || !sv_v.lv_ok || tick_end - sv_v.tv > sv_RATE * sv_field_max_age()) return;

#ifdef SCOUT_VIDEO_PROBE
    const int64_t rows_begin = esp_timer_get_time();
#endif
    const unsigned half_lines = sv_half_lines();
    const int64_t field256 = sv_v.field256;
    const int64_t line256 = field256 * 2 / half_lines;
    const int first = sv_v.pal ? 22 : 17, count = sv_v.pal ? 288 : 240;
    const int height = SCOUT_VIDEO_H;
    const int32_t need = (sv_v.pal ? 210 + 1040 : 188 + 1052) + 2;
    const uint32_t now_ms = (uint32_t)(now / 1000);
    for (unsigned j = 0; j < sv_s_np; ++j) {
        const sv_pulse_t *p = &sv_s_p[j];
        if (p->cls != sv_P_H || p->edge + need >= sv_SNAP - 2) continue;
        if (!sv_h_chain(j)) continue;
        const int64_t t = tick_end - (sv_SNAP - p->edge);
        int64_t phase = ((t - sv_v.tv) * 256) % field256;
        if (phase < 0) phase += field256;
        const int k = (int)((phase + line256 / 4) / line256) - first;
        if (k < 0 || k >= count) continue;
        const int row = k * height / count;
        const bool preferred = k == sv_preferred_source_line(row, count, height);
        if (!preferred && !sv_fallback_source_due(row, now_ms)) continue;
        const int32_t offset = sv_line_offset(j);
        if ((int32_t)p->edge + need + (offset + 255) / 256 >= (int32_t)sv_SNAP - 2) continue;
        const int32_t porch = sv_level(p->edge + 120, p->edge + 170);
        const int32_t depth = porch - sv_level(p->edge + 20, p->edge + 80);
        const int32_t expected = (sv_v.B16 - sv_v.S16) / 16;
        if (depth <= 0 || depth * 2 < expected || depth > expected * 2) continue;
        const unsigned quality = sv_active_video_quality(p->edge, porch, depth, sv_v.pal);
        if (quality == sv_LINE_REJECTED) continue;
        if (preferred) sv_v.preferred_ms[row] = now_ms;
        if (!sv_v.rows) sv_v.first_row_us = now;
        sv_draw_line(p->edge, offset, row, porch, depth, quality == sv_LINE_CLEAN);
        sv_v.last_row_us = now;
#ifdef SCOUT_VIDEO_PROBE
        sv_perf.decoded_rows++;
#endif
    }
#ifdef SCOUT_VIDEO_PROBE
    sv_perf.rows_us += esp_timer_get_time() - rows_begin;
#endif
}


static void scout_video_reset(void) {
    memset(&sv_v, 0, sizeof(sv_v));
    sv_probe_frames = 0;
    sv_v.pol = 1;
    sv_v.hper16 = sv_NTSC_LINE16;
}
static void scout_video_set_polarity(int polarity) {
    if (polarity) sv_v.pol = polarity;
    sv_probe_frames = 1;
}
static void scout_video_init(void) {
    /* Only CPU-owned completed snapshots use the RF bank. The spectrum FFT
     * may overwrite this workspace later; all scratch is rebuilt per capture. */
    uint8_t *base = (uint8_t *)(DUMP_BUF + HALF_WORDS);
    enum { level_offset = sv_U2N * sizeof(int32_t), pulse_offset = level_offset + sv_LN * sizeof(int32_t) };
    sv_s_u2 = (int32_t *)base;
    sv_s_lv = (int32_t *)(base + level_offset);
    sv_s_p = (sv_pulse_t *)(base + pulse_offset);
    _Static_assert(pulse_offset + sizeof(sv_pulse_t) * sv_MAXP <= HALF_WORDS * 4,
                   "video scratch must fit the upper completed-dump half");
    if (!sv_picture)
        sv_picture = heap_caps_malloc(CYD_MAX_W * SCOUT_MAX_VIDEO_H, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    scout_video_reset();
}
static bool scout_video_capture(void) {
    uint32_t elapsed;
#ifdef SCOUT_VIDEO_PROBE
    const int64_t begin = esp_timer_get_time();
#endif
    if (fast_capture(sv_SNAP, 2, &elapsed)) return false; /* 20 MS/s, finite snapshot */
#ifdef SCOUT_VIDEO_PROBE
    const int64_t complete = esp_timer_get_time();
    sv_perf.capture_us += complete - begin;
    sv_perf.captures++;
#endif
    sv_process_snapshot(opt_last_t0 + (sv_SNAP + sv_RATE / 2) / sv_RATE);
#ifdef SCOUT_VIDEO_PROBE
    sv_perf.decode_us += esp_timer_get_time() - complete;
#endif
    return true;
}
static bool scout_video_live(int64_t now) {
    if (!sv_v.rows || now - sv_v.last_row_us >= sv_FRAME_HOLD_US) return false;
    if (!sv_picture) return true;
    const unsigned height = SCOUT_VIDEO_H;
    return sv_v.rows * 20 >= height * 19 ||
           (now - sv_v.first_row_us >= sv_PARTIAL_PREVIEW_WAIT_US && sv_v.rows * 5 >= height * 4);
}
