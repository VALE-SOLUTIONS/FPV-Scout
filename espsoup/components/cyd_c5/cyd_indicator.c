/* SPDX-License-Identifier: GPL-3.0-or-later
 * NM-CYD-C5 onboard WS2812C, GPIO27. No RF transmitter is controlled here.
 */
#include "cyd_c5.h"
#include <stdatomic.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_timer.h"

static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
static esp_timer_handle_t timer;
static atomic_uint rate;
static atomic_bool test_requested;
static rmt_symbol_word_t frames[2][25];

static void tick(void *unused) {
    (void)unused;
    static int previous=-1;
    static int64_t test_start;
    int64_t now=esp_timer_get_time();
    if(atomic_exchange(&test_requested,false))test_start=now;
    unsigned hz=atomic_load(&rate);
    if(test_start) {
        int64_t elapsed=now-test_start;
        if(elapsed<1000000)hz=1;
        else if(elapsed<2000000)hz=5;
        else if(elapsed<3000000)hz=30;
        else test_start=0;
    }
    int on=hz && (now*hz)%1000000<500000;
    if(on==previous)return;
    const rmt_transmit_config_t cfg={.flags.queue_nonblocking=1};
    // Immutable frames stay alive until RMT completes; no shared pixel mutation.
    if(rmt_transmit(channel,encoder,frames[on],sizeof(frames[on]),&cfg)==ESP_OK)
        previous=on;
}

esp_err_t cyd_indicator_init(void) {
    if(timer)return ESP_OK;
    for(unsigned on=0;on<2;on++) {
        uint32_t grb=on ? 32u<<8 : 0; /* modest red brightness, GRB wire order */
        for(unsigned j=0;j<24;j++) {
            bool bit=(grb>>(23-j))&1;
            frames[on][j]=(rmt_symbol_word_t){.level0=1,.duration0=bit?8:4,
                .level1=0,.duration1=bit?4:8};
        }
        frames[on][24]=(rmt_symbol_word_t){.duration0=1500,.duration1=1500};
    }
    const rmt_tx_channel_config_t cfg={.gpio_num=27,.clk_src=RMT_CLK_SRC_DEFAULT,
        .resolution_hz=10000000,.mem_block_symbols=48,.trans_queue_depth=1};
    esp_err_t err=rmt_new_tx_channel(&cfg,&channel);
    if(err!=ESP_OK)return err;
    const rmt_copy_encoder_config_t copy={};
    if((err=rmt_new_copy_encoder(&copy,&encoder))!=ESP_OK)goto fail;
    if((err=rmt_enable(channel))!=ESP_OK)goto fail;
    const esp_timer_create_args_t args={.callback=tick,.name="scout_led",.skip_unhandled_events=true};
    if((err=esp_timer_create(&args,&timer))!=ESP_OK)goto disable;
    // 120 updates/s resolves the 60 edges/s needed for a 30 Hz blink.
    if((err=esp_timer_start_periodic(timer,8333))==ESP_OK)return ESP_OK;
    esp_timer_delete(timer);timer=NULL;
disable:
    rmt_disable(channel);
fail:
    if(encoder){rmt_del_encoder(encoder);encoder=NULL;}
    rmt_del_channel(channel);channel=NULL;
    return err;
}
void cyd_indicator_rate(unsigned hz) {atomic_store(&rate,hz>30?30:hz);}
void cyd_indicator_test(void) {if(timer)atomic_store(&test_requested,true);}
