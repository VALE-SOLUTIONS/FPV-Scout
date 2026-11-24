/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Relative spectral contrast in 0.01 dB. This is neither dBm nor VTX watts.
 * shortcut: uncalibrated 3..30 dB display scale; calibrate with real RF measurements.
 */
static unsigned scout_indicator_rate(bool active,int32_t contrast,int64_t age) {
    if(!active || age<0 || age>5000000)return 0;
    if(contrast<=300)return 1;
    if(contrast>=3000)return 30;
    return 1+(unsigned)(contrast-300)*29/2700;
}
