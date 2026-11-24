#!/usr/bin/env python3
"""Exercise the actual LED driver with mocked RMT/timer APIs; no RF claims."""
from pathlib import Path
import os, shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'espsoup/components/cyd_c5/cyd_indicator.c').read_text()
source='\n'.join(s for s in source.splitlines() if not s.startswith('#include'))
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef int esp_err_t;
typedef void *rmt_channel_handle_t,*rmt_encoder_handle_t,*esp_timer_handle_t;
typedef unsigned atomic_uint;
typedef bool atomic_bool;
#define atomic_load(p) (*(p))
#define atomic_store(p,v) (*(p)=(v))
static bool exchange(bool *p,bool v){bool old=*p;*p=v;return old;}
#define atomic_exchange exchange
#define ESP_OK 0
#define RMT_CLK_SRC_DEFAULT 0
typedef struct {unsigned level0,duration0,level1,duration1;}rmt_symbol_word_t;
typedef struct {struct {unsigned queue_nonblocking;}flags;}rmt_transmit_config_t;
typedef struct {int gpio_num,clk_src,resolution_hz,mem_block_symbols,trans_queue_depth;}rmt_tx_channel_config_t;
typedef struct {int unused;}rmt_copy_encoder_config_t;
typedef struct {void(*callback)(void*);const char *name;bool skip_unhandled_events;}esp_timer_create_args_t;
static int64_t clock_us;
static unsigned calls,last_red;
static bool busy;
static int64_t esp_timer_get_time(void){return clock_us;}
static int rmt_new_tx_channel(const rmt_tx_channel_config_t *c,void **out){assert(c->gpio_num==27 && c->resolution_hz==10000000 && c->mem_block_symbols==48);*out=(void*)1;return 0;}
static int rmt_new_copy_encoder(const rmt_copy_encoder_config_t*c,void**out){(void)c;*out=(void*)2;return 0;}
static int rmt_enable(void*c){assert(c);return 0;}
static int rmt_disable(void*c){(void)c;return 0;}
static int rmt_del_channel(void*c){(void)c;return 0;}
static int rmt_del_encoder(void*c){(void)c;return 0;}
static int esp_timer_create(const esp_timer_create_args_t*c,void**out){assert(c->skip_unhandled_events);*out=(void*)3;return 0;}
static int esp_timer_start_periodic(void*t,unsigned us){assert(t && us==8333);return 0;}
static int esp_timer_delete(void*t){(void)t;return 0;}
static int rmt_transmit(void*c,void*e,const void*p,size_t bytes,const rmt_transmit_config_t*cfg){
 assert(c && e && bytes==25*sizeof(rmt_symbol_word_t) && cfg->flags.queue_nonblocking);
 if(busy)return 1;
 const rmt_symbol_word_t*s=p;unsigned grb=0;
 for(unsigned j=0;j<24;j++){
  assert(s[j].level0==1 && s[j].level1==0 && s[j].duration0+s[j].duration1==12);
  grb=(grb<<1)|(s[j].duration0==8);
 }
 assert(!s[24].level0 && !s[24].level1 && s[24].duration0+s[24].duration1==3000);
 assert(grb==0 || grb==(32u<<8));last_red=grb!=0;calls++;return 0;
}
'''+source+r'''
int main(void){
 assert(cyd_indicator_init()==ESP_OK);assert(cyd_indicator_init()==ESP_OK);
 clock_us=100;tick(NULL);assert(!last_red);
 cyd_indicator_rate(1);tick(NULL);assert(last_red);
 clock_us=600000;tick(NULL);assert(!last_red);
 clock_us=1000000;busy=true;unsigned before=calls;tick(NULL);assert(calls==before);
 busy=false;tick(NULL);assert(last_red);
 cyd_indicator_rate(900);assert(atomic_load(&rate)==30);
 cyd_indicator_rate(0);tick(NULL);assert(!last_red);
 cyd_indicator_test();clock_us=2000000;tick(NULL);assert(last_red);
 clock_us=2600000;tick(NULL);assert(!last_red);
 clock_us=3050000;tick(NULL);assert(last_red);
 clock_us=3150000;tick(NULL);assert(!last_red);
 clock_us=4000000;tick(NULL);assert(last_red);
 clock_us=4020000;tick(NULL);assert(!last_red);
 clock_us=5000000;tick(NULL);assert(!last_red);
 puts("PASS: actual WS2812 red/off bitstream, reset timing, nonblocking retry, rate clamp and 1/5/30 Hz test expiry");
}
'''
with tempfile.TemporaryDirectory(prefix='scout-indicator-') as d:
    tmp=Path(d); (tmp/'test.c').write_text(code)
    cl=shutil.which('cl')
    if cl:
        exe=tmp/'test.exe';cmd=[cl,'/nologo','/std:c11','/O2',str(tmp/'test.c'),'/Fe:'+str(exe),'/Fo:'+str(tmp)+os.sep]
    else:
        cc=shutil.which('cc') or shutil.which('gcc') or shutil.which('clang');assert cc,'Activate a host C compiler'
        exe=tmp/'test';cmd=[cc,'-std=c11','-O2',str(tmp/'test.c'),'-o',str(exe)]
    subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
