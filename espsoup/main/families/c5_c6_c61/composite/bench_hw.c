/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bench_hw.h"
#include "cvbs.h"
#include "driver/gpio.h"
#include "driver/parlio_tx.h"
#include "esp_heap_caps.h"
#include <stdlib.h>

enum { DAC_LOW_GPIO = 4, DAC_HIGH_GPIO = 5, DAC_WIDTH = 2 };
static parlio_tx_unit_handle_t output;
static uint8_t *dma;
static size_t dma_capacity;
static bool enabled;

esp_err_t scout_bench_hw_open(size_t capacity, uint8_t **buffer)
{
    if (output) return ESP_ERR_INVALID_STATE;
    uint8_t *allocation = heap_caps_aligned_calloc(64, 1, capacity, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!allocation) return ESP_ERR_NO_MEM;
    const parlio_tx_unit_config_t cfg = {
        .clk_src = PARLIO_CLK_SRC_PLL_F240M, .clk_in_gpio_num = -1,
        .clk_out_gpio_num = -1, .valid_gpio_num = -1,
        .output_clk_freq_hz = CVBS_CLOCK, .data_width = DAC_WIDTH,
        .data_gpio_nums = {DAC_LOW_GPIO, DAC_HIGH_GPIO, -1, -1, -1, -1, -1, -1},
        .trans_queue_depth = 1, .max_transfer_size = capacity,
        .bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB,
    };
    esp_err_t err = parlio_new_tx_unit(&cfg, &output);
    if (err != ESP_OK) {
        free(allocation);
        return err;
    }
    dma = allocation;
    dma_capacity = capacity;
    *buffer = dma;
    return ESP_OK;
}

esp_err_t scout_bench_hw_start(size_t length, uint8_t blank)
{
    if (!output || enabled) return ESP_ERR_INVALID_STATE;
    if (!length || length > dma_capacity) return ESP_ERR_INVALID_ARG;
    esp_err_t err = parlio_tx_unit_enable(output);
    if (err != ESP_OK) return err;
    enabled = true;
    const parlio_transmit_config_t run = {.idle_value = blank, .flags.loop_transmission = true};
    return parlio_tx_unit_transmit(output, dma, length * 8, &run);
}

esp_err_t scout_bench_hw_pause(void)
{
    if (!output) return ESP_OK;
    if (enabled) {
        esp_err_t err = parlio_tx_unit_disable(output);
        if (err != ESP_OK) return err;
        enabled = false;
    }
    return parlio_tx_unit_wait_all_done(output, 100);
}

esp_err_t scout_bench_hw_close(void)
{
    if (!output) return ESP_OK;
    esp_err_t err = scout_bench_hw_pause();
    if (err != ESP_OK) return err;
    err = parlio_del_tx_unit(output);
    if (err != ESP_OK) return err;
    output = NULL;
    gpio_reset_pin(DAC_LOW_GPIO);
    gpio_reset_pin(DAC_HIGH_GPIO);
    gpio_set_pull_mode(DAC_LOW_GPIO, GPIO_FLOATING);
    gpio_set_pull_mode(DAC_HIGH_GPIO, GPIO_FLOATING);
    free(dma);
    dma = NULL;
    dma_capacity = 0;
    return ESP_OK;
}
