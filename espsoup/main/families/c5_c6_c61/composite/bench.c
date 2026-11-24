/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bench.h"
#include "bench_hw.h"

scout_bench_view_t scout_bench;
static unsigned counter;
static int64_t next_tick;

enum { DAC_WIDTH = 2 };

bool scout_bench_close(void)
{
    scout_bench.running = false;
    esp_err_t err = scout_bench_hw_close();
    if (err != ESP_OK) {
        scout_bench.error = err;
        return false;
    }
    scout_bench = (scout_bench_view_t){0};
    return true;
}

static esp_err_t play(cvbs_mode_t mode)
{
    cvbs_frame_t *frame = &scout_bench.frame;
    cvbs_configure(frame, frame->dma, mode, DAC_WIDTH);
    cvbs_sync(frame);
    pattern_draw(frame, scout_bench.pattern, counter);
    esp_err_t err = scout_bench_hw_start(cvbs_bytes(frame), frame->blank);
    scout_bench.running = err == ESP_OK;
    return err;
}

void scout_bench_open(void)
{
    if (!scout_bench_close()) return;
    counter = 0;
    next_tick = 0;
    cvbs_configure(&scout_bench.frame, NULL, CVBS_PAL, DAC_WIDTH);
    size_t capacity = cvbs_bytes(&scout_bench.frame);
    esp_err_t err = scout_bench_hw_open(capacity, &scout_bench.frame.dma);
    if (err == ESP_OK) err = play(CVBS_NTSC);
    if (err != ESP_OK && scout_bench_close()) scout_bench.error = err;
}

void scout_bench_select(pattern_t pattern)
{
    if (!scout_bench.running) return;
    scout_bench.pattern = pattern;
    pattern_draw(&scout_bench.frame, scout_bench.pattern, counter);
}

void scout_bench_next(void)
{
    scout_bench_select((scout_bench.pattern + 1) % PAT_COUNT);
}

void scout_bench_format(cvbs_mode_t mode)
{
    if (!scout_bench.running || mode == scout_bench.frame.mode) return;
    scout_bench.running = false;
    esp_err_t err = scout_bench_hw_pause();
    if (err == ESP_OK) err = play(mode);
    if (err != ESP_OK && scout_bench_close()) scout_bench.error = err;
}

bool scout_bench_levels(unsigned black, unsigned white)
{
    if (!scout_bench.running || !cvbs_set_levels(&scout_bench.frame, black, white)) return false;
    pattern_draw(&scout_bench.frame, scout_bench.pattern, counter);
    return true;
}

void scout_bench_step(int64_t now)
{
    if (!scout_bench.running || scout_bench.pattern != PAT_BOUNCE || now < next_tick) return;
    pattern_draw(&scout_bench.frame, scout_bench.pattern, ++counter);
    next_tick = now + 33000;
}
