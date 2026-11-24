/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cyd_c5.h"
#include "cyd_font_smooth_data.h"

static const cyd_smooth_font_t *smooth_font(cyd_font_t id) {
    return &cyd_smooth_fonts[(unsigned)id < 4 ? id : CYD_FONT_UI];
}
static const cyd_glyph_t *smooth_glyph(const cyd_smooth_font_t *f, unsigned c) {
    return &f->glyphs[(c >= 32 && c <= 126 ? c : '?') - 32];
}
int cyd_label_width(const char *text, cyd_font_t id) {
    const cyd_smooth_font_t *f = smooth_font(id);
    int width = 0;
    for (; *text; text++) width += smooth_glyph(f, (unsigned char)*text)->advance;
    return width;
}
static uint16_t smooth_blend(uint16_t fg, uint16_t bg, unsigned alpha) {
    unsigned f = (fg >> 8) | ((fg & 255) << 8), b = (bg >> 8) | ((bg & 255) << 8);
    unsigned r = (((f >> 11) * alpha + (b >> 11) * (15 - alpha) + 7) / 15);
    unsigned g = ((((f >> 5) & 63) * alpha + ((b >> 5) & 63) * (15 - alpha) + 7) / 15);
    unsigned blue = (((f & 31) * alpha + (b & 31) * (15 - alpha) + 7) / 15);
    unsigned c = (r << 11) | (g << 5) | blue;
    return (uint16_t)((c >> 8) | ((c & 255) << 8));
}
void cyd_label_overlay_row(uint16_t *pixels, int row_x, int row_y, int width,
                           int x, int y, const char *text, uint16_t fg, cyd_font_t id) {
    const cyd_smooth_font_t *f = smooth_font(id);
    for (; *text; text++) {
        const cyd_glyph_t *g = smooth_glyph(f, (unsigned char)*text);
        int gy = row_y - y - g->top, gx = x + g->left;
        if (gy >= 0 && gy < g->h) {
            for (int col = 0; col < g->w; col++) {
                int target = gx + col - row_x;
                if (target < 0 || target >= width) continue;
                unsigned p = gy * g->w + col;
                unsigned b = f->pixels[g->offset + p / 2];
                unsigned a = p & 1 ? b & 15 : b >> 4;
                if (a) pixels[target] = smooth_blend(fg, pixels[target], a);
            }
        }
        x += g->advance;
    }
}
void cyd_label(int x, int y, const char *text, uint16_t fg, uint16_t bg, cyd_font_t id) {
    const cyd_smooth_font_t *f = smooth_font(id);
    int x0 = x < 0 ? 0 : x, right = x + cyd_label_width(text, id);
    int top = y < 0 ? 0 : y, bottom = y + f->height;
    if (right > cyd_width()) right = cyd_width();
    if (bottom > cyd_height()) bottom = cyd_height();
    int w = right - x0;
    if (w <= 0 || top >= bottom) return;
    uint16_t palette[16];
    for (unsigned a = 0; a < 16; a++) palette[a] = smooth_blend(fg, bg, a);
    uint16_t *band = cyd_band();
    int step = CYD_MAX_W * CYD_BAND_LINES / w;
    for (int row = top; row < bottom; row += step) {
        int rows = bottom - row < step ? bottom - row : step;
        for (int j = 0; j < rows * w; j++) band[j] = bg;
        int pen = x;
        for (const char *p = text; *p; p++) {
            const cyd_glyph_t *g = smooth_glyph(f, (unsigned char)*p);
            int gx = pen + g->left, gy = y + g->top;
            int a = gx < x0 ? x0 : gx, b = gx + g->w > right ? right : gx + g->w;
            for (int yy = row; yy < row + rows; yy++) {
                if (yy < gy || yy >= gy + g->h) continue;
                for (int xx = a; xx < b; xx++) {
                    unsigned pixel = (yy - gy) * g->w + xx - gx;
                    unsigned packed = f->pixels[g->offset + pixel / 2];
                    unsigned alpha = (pixel & 1) ? packed & 15 : packed >> 4;
                    if (alpha) band[(yy - row) * w + xx - x0] = palette[alpha];
                }
            }
            pen += g->advance;
        }
        cyd_blit(x0, row, w, rows, band);
    }
}
