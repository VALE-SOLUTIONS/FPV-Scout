/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../main/families/c5_c6_c61/scout_indicator.h"
int main(void) {
    assert(!scout_indicator_rate(false,3000,0));
    assert(!scout_indicator_rate(true,3000,5000001));
    assert(!scout_indicator_rate(true,3000,-1));
    assert(scout_indicator_rate(true,-9000,0)==1);
    assert(scout_indicator_rate(true,300,0)==1);
    assert(scout_indicator_rate(true,3000,0)==30);
    assert(scout_indicator_rate(true,90000,0)==30);
    unsigned last=0;
    for(int db=-100;db<=4000;db++) {
        unsigned hz=scout_indicator_rate(true,db,5000000);
        assert(hz>=last && hz>=1 && hz<=30);last=hz;
    }
    puts("PASS: relative LED scale, 1..30 Hz bounds, monotonicity, inactive/stale gating");
}
