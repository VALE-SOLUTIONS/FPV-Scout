/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

/* Single-task ABI. The driver owns the DMA buffer until a successful close. */
esp_err_t scout_bench_hw_open(size_t capacity, uint8_t **buffer);
esp_err_t scout_bench_hw_start(size_t length, uint8_t blank);
esp_err_t scout_bench_hw_pause(void);
esp_err_t scout_bench_hw_close(void);
