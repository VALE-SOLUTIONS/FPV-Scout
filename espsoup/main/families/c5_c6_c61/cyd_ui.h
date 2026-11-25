/* SPDX-License-Identifier: GPL-3.0-or-later
 * FPV Scout: standalone analog FPV scanner on the NM-CYD-C5.
 * Reuses ESPsoup's receive-only RF engine and RockBase display driver.
 */
#pragma once
#include "cyd_c5.h"
#include "nvs.h"
#include "cyd_wallpaper.h"
#include "scout_version.h"
#include "fpv_scan.h"
#include "scout_video.h"
#include "scout_spectrum.h"
#include "scout_digital.h"
#include "scout_navigation.h"
#include "scout_signal_log.h"
#include "scout_log_storage.h"
#include "scout_indicator.h"
#include "scout_remote_id.h"
#include "scout_signal_cal.h"
#include "composite/bench.h"

typedef struct { const char *name; unsigned mhz; } scout_channel_t;
static const scout_channel_t scout_channels[] = {
    {"R1",5658},{"R2",5695},{"R3",5732},{"R4",5769},{"R5",5806},{"R6",5843},{"R7",5880},{"R8",5917},
    {"A1",5865},{"A2",5845},{"A3",5825},{"A4",5805},{"A5",5785},{"A6",5765},{"A7",5745},{"A8",5725},
    {"B1",5733},{"B2",5752},{"B3",5771},{"B4",5790},{"B5",5809},{"B6",5828},{"B7",5847},{"B8",5866},
    {"E1",5705},{"E2",5685},{"E3",5665},{"E4",5645},{"E5",5885},{"E6",5905},{"E7",5925},{"E8",5945},
    {"F1",5740},{"F2",5760},{"F3",5780},{"F4",5800},{"F5",5820},{"F6",5840},{"F7",5860},{"F8",5880},
    {"L1",5362},{"L2",5399},{"L3",5436},{"L4",5473},{"L5",5510},{"L6",5547},{"L7",5584},{"L8",5621}
};
#define SCOUT_COUNT (sizeof(scout_channels) / sizeof(scout_channels[0]))
#define SCOUT_CHANNELS_PER_BAND 8
enum { SCOUT_CONTENDERS=3, SCOUT_COMPARE_ROUNDS=2, SCOUT_COMPARE_DWELL_US=45000 };
enum { SCOUT_COMPARE_VIDEO_US=12000, SCOUT_COMPARE_VIDEO_MIN=4, SCOUT_NEARBY_MHZ=6 };
enum { SCOUT_RECHECK_LQ=81, SCOUT_RECHECK_GAIN=12, SCOUT_RECHECK_MIN_CHECKS=6 };
#define SCOUT_RECHECK_BAD_US 600000LL
#define SCOUT_RECHECK_MAX_US 1200000LL
#define SCOUT_RECHECK_COOLDOWN_US 10000000LL
typedef struct {
    fpv_scan_t sync;
    int32_t strength;
    unsigned channel, h_pairs, observations, valid_h, clean_lines, video_checks;
} scout_candidate_t;
#define SCOUT_SYNC_TIMEOUT_US 5000000LL
#define SCOUT_VIDEO_RECENT_US 250000LL
#define SCOUT_WATERFALL_ROWS 80
#define SCOUT_BAND_24_LOW 2400
#define SCOUT_BAND_24_HIGH 2490
typedef struct {
    int8_t dbfs;
    uint8_t quality, history;
    bool seen;
} scout_control_reading_t;

typedef struct {
    int64_t elrs_at, dji_at;
    uint16_t elrs_mhz, dji_mhz;
    uint8_t elrs_hops, elrs_sf, dji_matches, step, phase;
    unsigned passes, errors;
    scout_control_reading_t elrs, dji;
} scout_control_t;

enum { SCOUT_ALL_ANALOG, SCOUT_ALL_DIGITAL, SCOUT_ALL_CONTROL, SCOUT_ALL_RID };
#define SCOUT_ALL_RID_US 4100000LL
#define SCOUT_ALL_ANALOG_AGE_US 16000000LL
typedef struct {
    unsigned phase, analog_channel;
    int analog_polarity;
    int64_t analog_at, rid_until;
    scout_rf_track_t digital;
} scout_all_t;

static const struct { unsigned page; uint8_t stored; const char *label; } scout_start_pages[] = {
    {SCOUT_MENU, 1, "Main Menu"}, {SCOUT_SCAN_PAGE, 0, "Video Scan"},
    {SCOUT_CONTROL, 3, "Control Links"}, {SCOUT_REMOTE_ID, 4, "Remote ID"},
    {SCOUT_SPECTRUM, 5, "Spectrum"}, {SCOUT_SIGNAL_LOG, 6, "Signal Log"}, {SCOUT_SETTINGS, 7, "Settings"},
};
#define SCOUT_START_PAGE_COUNT (sizeof(scout_start_pages) / sizeof(scout_start_pages[0]))

static struct {
    bool up, host, dirty, touching, boot_down, tuned, manual, auto_rescan;
    uint8_t lq_samples;
    uint16_t lq_history;
    unsigned page, log_page;
    uint8_t start_page;
    scout_all_t all;
    scout_signal_cal_t cal;
    bool spectrum_valid, video_live, video_pal, video_full;
    uint8_t video_full_state;
    int32_t floor_est;
    unsigned spectrum_step;
    bool spectrum_held, spectrum_24, scan_waterfall, scan_plot_dirty;
    uint8_t waterfall_head, waterfall_count;
    uint8_t *waterfall;
    scout_rf_track_t digital;
    scout_control_t control;
    int64_t spectrum_tick, spectrum_updated;
    int16_t spectrum_work[SCOUT_SPEC_W], spectrum_plot[SCOUT_SPEC_W];
    unsigned channel, sweeps, skipped_mhz, errors, capture_errors, captures;
    uint16_t manual_mhz, tuner_mhz;
    uint8_t tuner_channel;
    bool tuner_open, tuner_direct;
    bool log_clear_confirm;
    int log_selection;
    char log_status[40];
    int64_t rid_log_seen[2];
    scout_candidate_t candidates[SCOUT_CONTENDERS + 1];
    unsigned candidate_count, compare_index, compare_round, last_compared;
    bool comparing, rechecking;
    unsigned recheck_channel;
    int64_t recheck_bad_at, recheck_after;
    int64_t compare_start, compare_until, compare_next_video, compare_us, lock_us;
    int64_t last_draw, sweep_start, sweep_us, sweep_label_at, next_check, hold_started;
    int16_t *bins;
    fpv_scan_t scan;
} scout;

static scout_channel_t scout_current_channel(void) {
    if (scout.manual_mhz) return (scout_channel_t){"Custom", scout.manual_mhz};
    return scout_channels[scout.rechecking ? scout.recheck_channel : scout.channel];
}

static const char *const scout_wallpaper_modes[] = {"MAIN", "ALL", "OFF"};
static const char *const scout_cal_names[] = {
    "Signal Calibration", "Analog Video", "Digital Video", "Control Links", "Remote ID",
};

static unsigned scout_control_probability(const scout_control_reading_t *signal, unsigned passes) {
    unsigned count = passes < 8 ? passes : 8;
    uint8_t history = signal->history;
    if (signal->seen) {
        history = (history << 1) | 1;
        if (count < 8) count++;
    }
    unsigned matches = 0;
    for (; history; history &= history - 1) matches++;
    // Beta(1,1) prior keeps the first observation from displaying 100%.
    return (100 * (matches + 1) + (count + 2) / 2) / (count + 2);
}

static bool scout_wallpaper_visible(void) {
    const cyd_wallpaper_mode_t mode = cyd_wallpaper_mode();
    return (mode == CYD_WALLPAPER_ALL ||
           (mode == CYD_WALLPAPER_MAIN && scout.page == SCOUT_MENU && !scout.host)) &&
           cyd_wallpaper_active();
}

/* Peak near the tuned channel relative to the most recent band-sweep floor. */
static int32_t scout_rssi(int64_t now) {
    if (!scout.spectrum_valid || !scout.spectrum_updated ||
        now - scout.spectrum_updated > 5000000) return INT32_MIN;
    const int khz = (int)scout_current_channel().mhz * 1000;
    int16_t peak = INT16_MIN;
    for (int x = scout_frequency_x(khz - 3000); x <= scout_frequency_x(khz + 3000); x++)
        if (scout.spectrum_plot[x] > peak) peak = scout.spectrum_plot[x];
    return peak == INT16_MIN ? INT32_MIN : (int32_t)peak - scout.floor_est;
}

static void scout_log_analog(int64_t now) {
    if (scout.page == SCOUT_SIGNAL_CAL) return;
    const int32_t strength = scout_rssi(now);
    scout_log_record(SCOUT_LOG_ANALOG, scout_current_channel().mhz,
                     strength == INT32_MIN ? SCOUT_LOG_NO_LEVEL : strength / 100, now);
}

static int scout_lq(void) {
    if (!scout.lq_samples) return -1;
    int good = 0;
    for (uint16_t bits = scout.lq_history; bits; bits &= bits - 1) good++;
    return (good * 100 + scout.lq_samples / 2) / scout.lq_samples;
}

static void scout_signal_labels(int64_t now, char rssi[12], char lq[8]) {
    const int32_t strength = scout_rssi(now);
    const int quality = scout_lq();
    if (strength == INT32_MIN) snprintf(rssi, 12, "--");
    else snprintf(rssi, 12, "%+ddB", (int)(strength / 100));
    if (quality < 0) snprintf(lq, 8, "--");
    else snprintf(lq, 8, "%d%%", quality);
}

static bool scout_single_scan_action(void) {
    return !scout.manual && scout.scan.mode != FPV_HIT && scout.scan.mode != FPV_LOCK;
}

static void scout_tune(void) {
    frequency_mhz = scout.manual_mhz ? scout.manual_mhz : scout_channels[scout.channel].mhz;
    c5_khz_offset = 0;
    rx_analog_filter = rx_bandwidth_dcap(20);
    rx_channel_mode = rx_bandwidth_phy_mode(20);
    rx_ready = false;
    prepare_rx();
    scout.tuned = true;
}

static int32_t scout_measure_signal(void) {
    /* Rank candidates using a consistent gain instead of comparing AGC output levels. */
    unsigned max_gain = gain_max();
    bool fixed_gain = gain_mode == GAIN_HARDWARE && max_gain != 0;
    if (fixed_gain) force_rx_gain(1, max_gain < 40 ? max_gain : 40, 0);
    uint32_t elapsed;
    const char *err = spec_compute(2, 256, 4, spec_bins, &elapsed);
    if (fixed_gain) gain_apply();
    return err ? INT32_MIN : scout_signal_score(spec_bins);
}

static void scout_clear_signal(void) {
    cyd_indicator_rate(0);
    scout_video_reset();
    scout.video_live = false;
    scout.next_check = 0;
    scout.tuned = false;
    scout.lq_history = scout.lq_samples = 0;
    scout.digital = (scout_rf_track_t){0};
    scout.comparing = false;
    scout.rechecking = false;
    scout.recheck_bad_at = scout.recheck_after = 0;
    scout.candidate_count = 0;
    scout.sweep_start = scout.sweep_label_at = 0;
    fpv_resume(&scout.scan);
    scout.dirty = true;
}

static void scout_start_auto(bool skip_current) {
    scout.skipped_mhz = skip_current ? scout_current_channel().mhz : 0;
    scout.manual = false;
    scout.manual_mhz = 0;
    scout.channel = 0;
    scout_clear_signal();
}

static void scout_start_recheck(int64_t now) {
    cyd_indicator_rate(0);
    scout.rechecking = true;
    scout.recheck_channel = scout.channel;
    scout.channel = 0;
    scout.candidate_count = 0;
    scout.sweep_start = now;
    scout.recheck_bad_at = 0;
    scout.tuned = false;
    scout.dirty = true;
}

static bool scout_recheck_due(int64_t now) {
    if (scout.manual || !scout.auto_rescan || scout.scan.mode != FPV_LOCK ||
        scout.lq_samples < 16 || scout_lq() > SCOUT_RECHECK_LQ || !scout.scan.last_sync ||
        now - scout.scan.last_sync >= 1000000) {
        scout.recheck_bad_at = 0;
        return false;
    }
    if (!scout.recheck_bad_at) scout.recheck_bad_at = now;
    return now >= scout.recheck_after && now - scout.recheck_bad_at >= SCOUT_RECHECK_BAD_US;
}

static void scout_step_manual(bool previous) {
    if (scout.manual_mhz) {
        int nearest = SCOUT_BAND_HIGH - SCOUT_BAND_LOW + 1;
        for (unsigned i = 0; i < SCOUT_COUNT; i++) {
            int difference = (int)scout_channels[i].mhz - scout.manual_mhz;
            if (previous) difference = -difference;
            if (difference > 0 && difference < nearest) {
                nearest = difference;
                scout.channel = i;
            }
        }
        scout.manual_mhz = 0;
    } else {
        scout.channel = (scout.channel + (previous ? SCOUT_COUNT - 1 : 1)) % SCOUT_COUNT;
    }
    scout_clear_signal();
}

static bool scout_tuner_valid(void) {
    return !scout.tuner_direct ||
           (scout.tuner_mhz >= SCOUT_BAND_LOW && scout.tuner_mhz <= SCOUT_BAND_HIGH);
}

static void scout_tuner_apply(void) {
    if (!scout_tuner_valid()) return;
    if (scout.tuner_direct) {
        scout.manual_mhz = scout.tuner_mhz;
        // Keep the selected preset if two band/channel names share this frequency.
        if (scout_channels[scout.channel].mhz == scout.tuner_mhz) scout.manual_mhz = 0;
        for (unsigned i = 0; scout.manual_mhz && i < SCOUT_COUNT; i++) {
            if (scout_channels[i].mhz == scout.tuner_mhz) {
                scout.channel = i;
                scout.manual_mhz = 0;
            }
        }
    } else {
        scout.channel = scout.tuner_channel;
        scout.manual_mhz = 0;
    }
    scout.tuner_open = false;
    scout.dirty = true;
}

static void scout_confirmed(void) {
    scout.scan.mode = FPV_LOCK;
    scout.hold_started = esp_timer_get_time();
    scout_log_analog(scout.hold_started);
    scout_video_set_polarity(scout.scan.polarity);
    scout.dirty = true;
}

static void scout_button(int x) {
    bool held = scout.scan.mode == FPV_LOCK;
    if (x >= cyd_width() / 2) {
        if (held) scout_start_auto(true);
    } else if (scout.manual || held) {
        scout_start_auto(false);
    } else {
        scout.manual = true;
        scout.skipped_mhz = 0;
        scout_clear_signal();
    }
}

static void scout_cal_clear(void) {
    free(scout.cal.quiet_plot);
    scout.cal = (scout_signal_cal_t){0};
}

static void scout_open_page(unsigned page) {
    if (scout.page == SCOUT_BENCH && !scout_bench_close()) {
        scout.dirty = true;
        return;
    }
    const bool from_menu = scout.page == SCOUT_MENU;
    if (scout.page == SCOUT_REMOTE_ID || scout.page == SCOUT_SIGNAL_CAL ||
        (from_menu && page != SCOUT_REMOTE_ID)) scout_rid_stop();
    if (scout.page == SCOUT_SIGNAL_CAL || page == SCOUT_SIGNAL_CAL) scout_cal_clear();
    if (scout.page == SCOUT_SPECTRUM || scout.page == SCOUT_SCAN_PAGE || scout.page == SCOUT_CONTROL) {
        free(scout.waterfall);
        scout.waterfall = NULL;
    }
    scout.page = page;
    scout.video_full = false;
    if (page == SCOUT_SIGNAL_LOG) {
        scout.log_page = 0;
        scout.log_selection = -1;
        scout.log_clear_confirm = false;
        scout.log_status[0] = '\0';
        if (scout_log_dirty) scout_log_urgent = true;
    }
    if ((page == SCOUT_CONTROL && !from_menu) || page == SCOUT_MENU)
        scout.control = (scout_control_t){0};
    scout.tuner_open = false;
    scout.spectrum_valid = false;
    scout.spectrum_held = false;
    scout.waterfall_head = scout.waterfall_count = 0;
    if (page == SCOUT_SPECTRUM || page == SCOUT_SCAN_PAGE || page == SCOUT_CONTROL)
        scout.waterfall = heap_caps_malloc(SCOUT_SPEC_W * SCOUT_WATERFALL_ROWS, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!scout.waterfall && page == SCOUT_SCAN_PAGE) scout.scan_waterfall = false;
    scout.spectrum_step = 0;
    scout.spectrum_tick = 0;
    scout.spectrum_updated = 0;
    scout.skipped_mhz = 0;
    if (page == SCOUT_SCAN_PAGE && !scout.manual) scout.channel = 0;
    if (page != SCOUT_MENU) scout_clear_signal();
    if (page == SCOUT_BENCH) scout_bench_open();
    if (page == SCOUT_REMOTE_ID) {
        if (!from_menu) scout.rid_log_seen[0] = scout.rid_log_seen[1] = 0;
        scout_rid_start();
    }
    if (page == SCOUT_MENU) {
        scout.all = (scout_all_t){0};
        scout.rid_log_seen[0] = scout.rid_log_seen[1] = 0;
        scout_start_auto(false);
    }
}
static void scout_load_rescan(void) {
    nvs_handle_t nvs;
    uint8_t enabled = 1;
    if (nvs_open("cyd", NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_u8(nvs, "auto_rescan", &enabled) != ESP_OK) enabled = 1;
        nvs_close(nvs);
    }
    scout.auto_rescan = enabled != 0;
}
static void scout_toggle_rescan(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cyd", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, "auto_rescan", !scout.auto_rescan);
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (err == ESP_OK) {
        scout.auto_rescan = !scout.auto_rescan;
        scout.dirty = true;
    } else {
        printf("SCOUT auto rescan save failed: %d\n", (int)err);
    }
}
static void scout_load_start_page(void) {
    nvs_handle_t nvs;
    uint8_t choice = 1;
    if (nvs_open("cyd", NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_u8(nvs, "start_page", &choice) != ESP_OK) choice = 1;
        nvs_close(nvs);
    }
    /* Scan All and Main Menu were separate choices in 1.18.0. */
    if (choice == 2) choice = 1;
    scout.start_page = 0;
    for (unsigned i = 0; i < SCOUT_START_PAGE_COUNT; ++i)
        if (scout_start_pages[i].stored == choice) scout.start_page = i;
}

static void scout_next_start_page(void) {
    const uint8_t next = (scout.start_page + 1) % SCOUT_START_PAGE_COUNT;
    nvs_handle_t nvs;
    if (nvs_open("cyd", NVS_READWRITE, &nvs) != ESP_OK) return;
    esp_err_t err = nvs_set_u8(nvs, "start_page", scout_start_pages[next].stored);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err == ESP_OK) {
        scout.start_page = next;
        scout.dirty = true;
    }
}

static void scout_open_video_result(bool digital) {
    const int64_t now = esp_timer_get_time();
    const bool analog = !digital && scout.all.analog_at &&
                        now - scout.all.analog_at < SCOUT_ALL_ANALOG_AGE_US;
    const unsigned mhz = digital ? scout.all.digital.shown.mhz : 0;
    scout.manual = mhz != 0;
    scout.manual_mhz = mhz;
    scout_open_page(SCOUT_SCAN_PAGE);
    if (analog) {
        scout.channel = scout.all.analog_channel;
        scout.scan = (fpv_scan_t){.mode=FPV_VERIFY, .polarity=scout.all.analog_polarity,
                                   .deadline=esp_timer_get_time()+400000};
    }
    if (mhz) {
        scout.digital = scout.all.digital;
        scout.digital.best = (scout_rf_hit_t){0};
    }
}

static void scout_navigation_action(int action) {
    switch(action) {
    case SCOUT_NAV_MENU: scout_open_page(SCOUT_MENU); break;
    case SCOUT_NAV_SCAN: scout_open_page(SCOUT_SCAN_PAGE); break;
    case SCOUT_NAV_HOME_ANALOG: scout_open_video_result(false); break;
    case SCOUT_NAV_HOME_DIGITAL: scout_open_video_result(true); break;
    case SCOUT_NAV_CONTROL: scout_open_page(SCOUT_CONTROL); break;
    case SCOUT_NAV_REMOTE_ID: scout_open_page(SCOUT_REMOTE_ID); break;
    case SCOUT_NAV_SIGNAL_LOG: scout_open_page(SCOUT_SIGNAL_LOG); break;
    case SCOUT_NAV_SPECTRUM: scout_open_page(SCOUT_SPECTRUM); break;
    case SCOUT_NAV_SETTINGS: scout_open_page(SCOUT_SETTINGS); break;
    case SCOUT_NAV_SIGNAL_CAL: scout_open_page(SCOUT_SIGNAL_CAL); break;
    case SCOUT_NAV_BENCH: scout_open_page(SCOUT_BENCH); break;
    case SCOUT_NAV_SCAN_DISPLAY:
        if (scout.waterfall) {
            scout.scan_waterfall = !scout.scan_waterfall;
            scout.scan_plot_dirty = true;
        }
        break;
    case SCOUT_NAV_CALIBRATE: cyd_touch_calibrate(true); scout.dirty=true; break;
    case SCOUT_NAV_LED_TEST: cyd_indicator_test(); break;
    case SCOUT_NAV_AUTO_RESCAN: scout_toggle_rescan(); break;
    case SCOUT_NAV_START_PAGE: scout_next_start_page(); break;
    case SCOUT_NAV_WALLPAPER:
        cyd_wallpaper_next_file();
        scout.dirty = true;
        break;
    case SCOUT_NAV_WALLPAPER_MODE:
        cyd_wallpaper_next_mode();
        scout.dirty = true;
        break;
    case SCOUT_NAV_ORIENTATION:
        cyd_set_landscape(!cyd_landscape());
        cyd_touch_calibrate(false);
        scout.dirty = true;
        break;
    case SCOUT_NAV_LEFT:
        if (scout.page == SCOUT_BENCH) {
            if (!scout_bench.running) break;
            scout_bench_format(scout_bench.frame.mode == CVBS_NTSC ? CVBS_PAL : CVBS_NTSC);
            scout.dirty = true;
        } else if (scout.page == SCOUT_SPECTRUM) {
            scout.spectrum_held = !scout.spectrum_held;
            scout.dirty = true;
        } else scout_button(0);
        break;
    case SCOUT_NAV_RIGHT:
        if (scout.page == SCOUT_BENCH) {
            if (!scout_bench.running) break;
            scout_bench_next();
            scout.dirty = true;
        } else if (scout.page == SCOUT_SPECTRUM) {
            scout.spectrum_24 = !scout.spectrum_24;
            scout.spectrum_held = false;
            scout.spectrum_valid = false;
            scout.spectrum_step = 0;
            scout.waterfall_head = scout.waterfall_count = 0;
            scout.dirty = true;
        } else if (scout.manual) scout_step_manual(false);
        else scout_button(cyd_width());
        break;
    case SCOUT_NAV_PREVIOUS:
        if (scout.manual) scout_step_manual(true);
        else scout_button(cyd_width());
        break;
    case SCOUT_NAV_CHANNEL_CENTER:
        if (scout.manual) {
            scout.tuner_open = true;
            scout.tuner_direct = scout.manual_mhz != 0;
            scout.tuner_mhz = 0;
            scout.tuner_channel = scout.channel;
            scout_clear_signal();
        } else scout_button(cyd_width());
        break;
    default: break;
    }
}

#define SCOUT_TUNER_TAB_Y 44
#define SCOUT_TUNER_KEY_Y (cyd_landscape() ? 101 : 122)
#define SCOUT_TUNER_KEY_H (cyd_landscape() ? 28 : 42)
#define SCOUT_TUNER_PITCH (SCOUT_TUNER_KEY_H + 6)

static int scout_tuner_edge(int column, int columns) {
    return 12 + (cyd_width() - 18) * column / columns;
}

static int scout_tuner_column(int x, int columns) {
    for (int column = 0; column < columns; column++)
        if (x >= scout_tuner_edge(column, columns) && x < scout_tuner_edge(column + 1, columns) - 6)
            return column;
    return -1;
}

static void scout_tuner_input(int x, int y) {
    if (x >= 8 && x < 66 && y >= 6 && y < 34) {
        scout.tuner_open = false;
    } else if (y >= SCOUT_TUNER_TAB_Y && y < SCOUT_TUNER_TAB_Y + SCOUT_BUTTON_H) {
        int tab = scout_tuner_column(x, 2);
        if (tab < 0) return;
        scout.tuner_direct = tab == 1;
    } else if (x >= 12 && x < cyd_width() - 12 && y >= SCOUT_BUTTON_Y && y < SCOUT_BUTTON_Y + SCOUT_BUTTON_H) {
        scout_tuner_apply();
        return;
    } else if (y >= SCOUT_TUNER_KEY_Y && y < SCOUT_TUNER_KEY_Y + 3 * SCOUT_TUNER_PITCH) {
        int dy = y - SCOUT_TUNER_KEY_Y, row = dy / SCOUT_TUNER_PITCH;
        const int bands = SCOUT_COUNT / SCOUT_CHANNELS_PER_BAND;
        int col = scout_tuner_column(x, !scout.tuner_direct && row == 0 ? bands : 4);
        if (col < 0 || dy % SCOUT_TUNER_PITCH >= SCOUT_TUNER_KEY_H) return;
        if (!scout.tuner_direct) {
            if (row == 0)
                scout.tuner_channel = col * SCOUT_CHANNELS_PER_BAND + scout.tuner_channel % SCOUT_CHANNELS_PER_BAND;
            else
                scout.tuner_channel = scout.tuner_channel / SCOUT_CHANNELS_PER_BAND * SCOUT_CHANNELS_PER_BAND +
                                      (row - 1) * 4 + col;
        } else if (row == 0 && col == 3) {
            scout.tuner_mhz = 0;
        } else if (row == 1 && col == 3) {
            scout.tuner_mhz /= 10;
        } else if (scout.tuner_mhz < 1000) {
            int digit = col == 3 ? 0 : row * 3 + col + 1;
            scout.tuner_mhz = scout.tuner_mhz * 10 + digit;
        }
    } else return;
    scout.dirty = true;
}

static void scout_log_input(int x, int y) {
    const int width = cyd_width(), height = cyd_height();
    const int action = scout_navigation(SCOUT_SIGNAL_LOG,x,y,cyd_landscape());
    if (action == SCOUT_NAV_MENU) {
        if (scout.log_selection >= 0 || scout.log_clear_confirm) {
            scout.log_selection = -1;
            scout.log_clear_confirm = false;
        } else scout_open_page(SCOUT_MENU);
    } else if (scout.log_selection >= 0) return;
    else if (action == SCOUT_NAV_LEFT) {
        if (scout.log_clear_confirm) scout.log_clear_confirm = false;
        else if (scout_log.count) {
            scout_log_save_to_sd(scout.log_status,sizeof(scout.log_status));
            if (!scout_log.count) scout.log_page = 0;
        }
    } else if (action == SCOUT_NAV_RIGHT) {
        if (scout.log_clear_confirm) {
            scout.log_clear_confirm = false;
            bool cleared = scout_log_clear();
            if (cleared) scout.log_page = 0;
            snprintf(scout.log_status,sizeof(scout.log_status),"%s",cleared ? "Log cleared" : "Clear failed. Log kept.");
        } else if (scout_log.count) scout.log_clear_confirm = true;
    } else if (!scout.log_clear_confirm && y >= 36 && y < 62) {
        if (x >= 12 && x < 82 && scout.log_page) scout.log_page--;
        if (x >= width - 82 && x < width - 12 &&
            (scout.log_page + 1) * SCOUT_LOG_ROWS(height) < scout_log.count) scout.log_page++;
    } else if (!scout.log_clear_confirm && y >= SCOUT_LOG_ROW_Y && x >= 12 && x < width - 12) {
        const unsigned row = (y - SCOUT_LOG_ROW_Y) / SCOUT_LOG_ROW_H;
        const unsigned index = scout.log_page * SCOUT_LOG_ROWS(height) + row;
        if (row < SCOUT_LOG_ROWS(height) && index < scout_log.count &&
            (y - SCOUT_LOG_ROW_Y) % SCOUT_LOG_ROW_H < 32) scout.log_selection = index;
        else return;
    } else return;
    scout.dirty = true;
}

static void scout_toggle_fullscreen(void) {
    scout.video_full = !scout.video_full;
    scout.video_full_state = 0;
    scout_video_invalidate();
    scout.dirty = true;
}

static const char *const scout_cal_keys[] = {
    "", "sigcal_ana_v1", "sigcal_dig_v1", "sigcal_ctl_v1", "sigcal_rid_v1",
};

static void scout_cal_select(unsigned tool) {
    scout_cal_clear();
    scout.cal.tool = tool;
    nvs_handle_t nvs;
    if (nvs_open("cyd", NVS_READONLY, &nvs) == ESP_OK) {
        size_t size = sizeof(scout.cal.saved);
        esp_err_t err = nvs_get_blob(nvs, scout_cal_keys[tool], &scout.cal.saved, &size);
        nvs_close(nvs);
        scout.cal.saved_valid = err == ESP_OK && size == sizeof(scout.cal.saved) &&
                                scout.cal.saved.signal > 0 && scout.cal.saved.signal <= 3 &&
                                scout.cal.saved.quality <= 100;
    }
    scout.dirty = true;
}

static void scout_cal_save(void) {
    scout_signal_cal_t *cal = &scout.cal;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cyd", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs, scout_cal_keys[cal->tool], &cal->result, sizeof(cal->result));
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    cal->save_failed = err != ESP_OK;
    if (!cal->save_failed) {
        cal->saved = cal->result;
        cal->saved_valid = true;
        cal->stage = SCOUT_CAL_SAVED;
    }
    scout.dirty = true;
}

static void scout_cal_finish(bool passed) {
    if (scout.cal.tool == SCOUT_CAL_RID) scout_rid_stop();
    scout.cal.stage = passed ? SCOUT_CAL_REVIEW : SCOUT_CAL_FAILED;
    scout.dirty = true;
}

static void scout_cal_begin(void) {
    scout_signal_cal_t *cal = &scout.cal;
    scout.spectrum_step = 0;
    scout.spectrum_valid = false;
    cal->baseline_error = false;
    cal->save_failed = false;
    cal->next_sample = 0;
    cal->result = (scout_cal_reference_t){.floor=cal->baseline_ready ? cal->result.floor : 0};
    const int64_t now = esp_timer_get_time();
    if (cal->tool != SCOUT_CAL_RID && !cal->baseline_ready) {
        if (cal->tool == SCOUT_CAL_DIGITAL) {
            if (!cal->quiet_plot)
                cal->quiet_plot = heap_caps_malloc(sizeof(scout.spectrum_plot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!cal->quiet_plot) {
                scout_cal_finish(false);
                return;
            }
            for (unsigned i = 0; i < SCOUT_SPEC_W; ++i) cal->quiet_plot[i] = INT16_MIN;
        }
        cal->baseline_passes = 0;
        cal->baseline_total = 0;
        cal->stage = SCOUT_CAL_BASELINE;
        cal->deadline = now + 3000000;
    } else {
        cal->stage = SCOUT_CAL_CHECK;
        cal->deadline = now + (cal->tool == SCOUT_CAL_RID ? 24000000 : 12000000);
        switch (cal->tool) {
        case SCOUT_CAL_ANALOG: scout_start_auto(false); break;
        case SCOUT_CAL_DIGITAL: scout.digital = (scout_rf_track_t){0}; break;
        case SCOUT_CAL_CONTROL: scout.control = (scout_control_t){0}; break;
        case SCOUT_CAL_RID:
            scout_rid_start();
            if (!scout_rid) scout_cal_finish(false);
            break;
        }
    }
    scout.dirty = true;
}

static void scout_cal_input(int x, int y) {
    scout_signal_cal_t *cal = &scout.cal;
    if (x >= 8 && x < 66 && y >= 6 && y < 34) {
        if (cal->tool == SCOUT_CAL_MENU) scout_open_page(SCOUT_SETTINGS);
        else {
            if (cal->tool == SCOUT_CAL_RID) scout_rid_stop();
            scout_cal_clear();
            scout.dirty = true;
        }
        return;
    }
    if (x < 16 || x >= cyd_width() - 16) return;
    if (cal->tool == SCOUT_CAL_MENU) {
        const int row = y - SCOUT_CAL_ROW_Y;
        if (row >= 0 && row < 4 * SCOUT_CAL_ROW_PITCH && row % SCOUT_CAL_ROW_PITCH < SCOUT_CAL_ROW_H)
            scout_cal_select(SCOUT_CAL_ANALOG + row / SCOUT_CAL_ROW_PITCH);
        return;
    }
    if (y < SCOUT_BUTTON_Y || y >= SCOUT_BUTTON_Y + SCOUT_BUTTON_H) return;
    switch (cal->stage) {
    case SCOUT_CAL_SETUP:
    case SCOUT_CAL_READY:
    case SCOUT_CAL_FAILED: scout_cal_begin(); break;
    case SCOUT_CAL_CHECK:
        if (cal->tool == SCOUT_CAL_RID && cal->result.signal) scout_cal_finish(true);
        break;
    case SCOUT_CAL_REVIEW: scout_cal_save(); break;
    case SCOUT_CAL_SAVED: scout_cal_select(cal->tool); break;
    }
}

static void scout_input(void) {
    int x, y;
    bool down = cyd_touch(&x, &y);
    if (down && !scout.touching && scout.tuner_open) {
        scout_tuner_input(x, y);
    } else if (down && !scout.touching && scout.page == SCOUT_SIGNAL_LOG) {
        scout_log_input(x,y);
    } else if (down && !scout.touching && scout.page == SCOUT_SIGNAL_CAL) {
        scout_cal_input(x,y);
    } else if (down && !scout.touching && scout.page == SCOUT_SCAN_PAGE &&
        (scout.video_full || (scout.video_live && y >= SCOUT_VIDEO_Y && y < SCOUT_VIDEO_Y + SCOUT_VIDEO_H))) {
        scout_toggle_fullscreen();
    } else if (down && !scout.touching) {
        const bool single = scout.page == SCOUT_SCAN_PAGE && scout_single_scan_action();
        const int action = single && x >= 12 && x < cyd_width() - 12 &&
                           y >= SCOUT_BUTTON_Y && y < SCOUT_BUTTON_Y + SCOUT_BUTTON_H
            ? SCOUT_NAV_LEFT : scout_navigation(scout.page, x, y, cyd_landscape());
        scout_navigation_action(action);
    }
    scout.touching = down;
    bool boot = cyd_boot_pressed();
    if (boot && !scout.boot_down && scout.page==SCOUT_SCAN_PAGE && !scout.video_full) {
        if (scout.scan.mode == FPV_HIT || scout.scan.mode == FPV_LOCK) scout_start_auto(false);
    }
    scout.boot_down = boot;
}
static int scout_corner_inset(int radius, int row) {
    int inset = radius;
    while (inset > 0 && (radius - inset) * (radius - inset) +
           (radius - row - 1) * (radius - row - 1) <= radius * radius) inset--;
    return inset + 1;
}

static void scout_round(int x, int y, int w, int h, int radius, uint16_t colour) {
    cyd_fill(x, y + radius, w, h - 2 * radius, colour);
    for (int row = 0; row < radius; row++) {
        int inset = scout_corner_inset(radius, row);
        cyd_hline(x + inset, y + row, w - 2 * inset, colour);
        cyd_hline(x + inset, y + h - row - 1, w - 2 * inset, colour);
    }
}
static void scout_center(int x, int y, int width, const char *text, uint16_t fg, uint16_t bg) {
    cyd_label(x + (width - cyd_label_width(text, CYD_FONT_UI)) / 2, y, text, fg, bg, CYD_FONT_UI);
}

typedef struct {
    int x, y;
    const char *text;
    uint16_t color;
    cyd_font_t font;
} scout_bg_label_t;

static void scout_labels_row(uint16_t *row, int x, int y, int width,
                             const scout_bg_label_t *labels, size_t count) {
    for (size_t i = 0; i < count; i++)
        cyd_label_overlay_row(row, x, y, width, labels[i].x, labels[i].y,
                              labels[i].text, labels[i].color, labels[i].font);
}

static void scout_strip(int from, int to, uint16_t bg, bool wallpaper,
                        const scout_bg_label_t *labels, size_t count) {
    const int width = cyd_width(), height = cyd_height();
    uint16_t *band = cyd_band();
    for (int y = from; y < to; y += CYD_BAND_LINES) {
        int rows = to - y < CYD_BAND_LINES ? to - y : CYD_BAND_LINES;
        for (int r = 0; r < rows; r++) {
            uint16_t *row = band + r * width;
            if (wallpaper) cyd_wallpaper_row(row, width, height, y + r);
            else for (int x = 0; x < width; x++) row[x] = bg;
            scout_labels_row(row, 0, y + r, width, labels, count);
        }
        cyd_blit(0, y, width, rows, band);
    }
}
static void scout_background(int from, int to, uint16_t bg,
                             const scout_bg_label_t *labels, size_t count) {
    scout_strip(from, to, bg, scout_wallpaper_visible(), labels, count);
}

static void scout_pill(int x, int y, int width, int height, uint16_t color,
                       const scout_bg_label_t *labels, size_t count) {
    uint16_t *row = cyd_band();
    for (int r = 0; r < height; r++) {
        int edge = r < height - 1 - r ? r : height - 1 - r;
        int inset = edge < 8 ? scout_corner_inset(8, edge) : 0;
        int span = width - 2 * inset;
        for (int i = 0; i < span; i++) row[i] = color;
        scout_labels_row(row, x + inset, y + r, span, labels, count);
        cyd_blit(x + inset, y + r, span, 1, row);
    }
}

static int scout_marker_x(int width) {
    int bin = scout_frequency_x((int)scout_current_channel().mhz * 1000);
    return (bin * (width - 1) + (SCOUT_SPEC_W - 1) / 2) / (SCOUT_SPEC_W - 1);
}
static void scout_plot_label(uint16_t *row, int y, int width, const char *view) {
    if (!view || y < 3 || y >= 21) return;
    const int box = cyd_label_width(view, CYD_FONT_SMALL) + 10;
    const int left = width - box - 3, line = y - 3;
    const int edge = line < 17 - line ? line : 17 - line;
    const int inset = edge < 4 ? scout_corner_inset(4, edge) : 0;
    for (int x = left + inset; x < left + box - inset; x++) row[x] = CYD_RGB(27,38,54);
    cyd_label_overlay_row(row, 0, y, width, left + 5, 5, view, CYD_WHITE, CYD_FONT_SMALL);
}
static void scout_spectrum_draw(int H, const char *view) {
    const int X = SCOUT_SPEC_X, Y = SCOUT_SPEC_Y, W = cyd_width() - 2 * X;
    const uint16_t bg = CYD_RGB(10,14,20), grid = CYD_RGB(25,36,49);
    const uint16_t fill = CYD_RGB(18,65,69), trace = CYD_RGB(64,218,190);
    uint8_t top[CYD_MAX_W];
    const int marker = scout_marker_x(W);
    for (int x = 0; x < W; x++) {
        int bin = x * SCOUT_SPEC_W / W;
        int32_t value = scout.spectrum_plot[bin];
        int row = H - 1 - (value - scout.floor_est + 500) * (H - 1) / 6000;
        top[x] = !scout.spectrum_valid || value == INT16_MIN ? UINT8_MAX :
                 row < 0 ? 0 : row >= H ? H - 1 : row;
    }
    uint16_t *pixels = cyd_band();
    for (int r0 = 0; r0 < H; r0 += CYD_BAND_LINES) {
        int rows = H - r0 < CYD_BAND_LINES ? H - r0 : CYD_BAND_LINES;
        for (int r = 0; r < rows; r++) {
            int y = r0 + r;
            uint16_t *row = pixels + r * W;
            for (int x = 0; x < W; x++) {
                uint16_t color = y % 12 == 0 || x == W/2 ? grid : bg;
                if (top[x] != UINT8_MAX) {
                    if (y > top[x]) color = fill;
                    if (y == top[x] || (x && top[x-1] != UINT8_MAX && ((y > top[x] && y <= top[x-1]) ||
                        (y < top[x] && y >= top[x-1])))) color = trace;
                }
                if (view && x == marker && y%4<2) color=CYD_RGB(255,192,96);
                row[x] = color;
            }
            scout_plot_label(row, y, W, view);
        }
        cyd_blit(X, Y + r0, W, rows, pixels);
    }
}
static void scout_waterfall_draw(int y, int height, const char *view) {
    const int width = cyd_width() - 2 * SCOUT_SPEC_X;
    const int marker = scout_marker_x(width);
    const int history = height < SCOUT_WATERFALL_ROWS ? height : SCOUT_WATERFALL_ROWS;
    uint16_t *pixels = cyd_band();
    for (int top = 0; top < height; top += CYD_BAND_LINES) {
        const int rows = height - top < CYD_BAND_LINES ? height - top : CYD_BAND_LINES;
        for (int r = 0; r < rows; r++) {
            const unsigned age = (top + r) * history / height;
            const unsigned index = (scout.waterfall_head + SCOUT_WATERFALL_ROWS - 1 - age) % SCOUT_WATERFALL_ROWS;
            const uint8_t *values = scout.waterfall && age < scout.waterfall_count ?
                scout.waterfall + index * SCOUT_SPEC_W : NULL;
            for (int x = 0; x < width; x++) {
                const unsigned value = values ? values[x * SCOUT_SPEC_W / width] : 0;
                pixels[r * width + x] = view && x == marker && (top + r) % 4 < 2 ?
                    CYD_RGB(255,192,96) : CYD_RGB(6+value/2, 12+value*3/4, 22+value/2);
            }
            scout_plot_label(pixels + r * width, top + r, width, view);
        }
        cyd_blit(SCOUT_SPEC_X, y + top, width, rows, pixels);
    }
}
static void scout_draw_scan_plot(void) {
    if (scout.video_full) return;
    if (scout.scan_waterfall) scout_waterfall_draw(SCOUT_SPEC_Y, SCOUT_SPEC_H, "Waterfall");
    else scout_spectrum_draw(SCOUT_SPEC_H, "Sweep");
    scout.scan_plot_dirty = false;
}
#ifdef SCOUT_RUST_CORE
static bool scout_cal_analog_sync(unsigned mhz) {
    const unsigned saved_freq = frequency_mhz, saved_mode = rx_channel_mode;
    const int saved_filter = rx_analog_filter;
    frequency_mhz = mhz;
    rx_channel_mode = rx_bandwidth_phy_mode(20);
    rx_analog_filter = rx_bandwidth_dcap(20);
    rx_ready = false;
    prepare_rx();
    bool analog = false;
    for (unsigned i = 0; i < 2 && !analog; ++i) {
        uint32_t elapsed;
        if (!fast_capture(FPV_SAMPLES,2,&elapsed)) {
            const fpv_evidence_t e = fpv_detect(DUMP_BUF,scout.bins);
            analog = e.h_pairs >= 4 || e.v_pairs >= 2;
        }
    }
    frequency_mhz = saved_freq;
    rx_channel_mode = saved_mode;
    rx_analog_filter = saved_filter;
    rx_ready = false;
    prepare_rx();
    return analog;
}

static void scout_rf_probe(unsigned centre) {
    const bool cal_digital = scout.page == SCOUT_SIGNAL_CAL && scout.cal.tool == SCOUT_CAL_DIGITAL &&
                             scout.cal.stage == SCOUT_CAL_CHECK;
    if (!cal_digital && scout.page != SCOUT_MENU && (scout.page != SCOUT_SCAN_PAGE || scout.comparing ||
        scout.scan.mode != FPV_SCAN || scout.candidate_count)) return;

    scout_rf_shape_t shape;
    scout_rf_shape(spec_bins, &shape);
    if (!shape.width_khz) return;
    const int estimate = (int)centre * 1000 + shape.offset_khz;
    const unsigned mhz = (estimate + 500) / 1000;
    if (cal_digital && !scout_cal_peak_rise(scout.cal.quiet_plot,scout.spectrum_work,
        scout_frequency_x((int)mhz*1000-3000),scout_frequency_x((int)mhz*1000+3000))) return;
    if (mhz < 5650 || mhz > 5925) return;
    if (scout.manual) {
        const unsigned tuned = scout_current_channel().mhz;
        const unsigned distance_khz = (tuned > mhz ? tuned - mhz : mhz - tuned) * 1000;
        if (distance_khz > shape.width_khz / 2 + 1500) return;
    }
    // Receiver-centered noise and spurs move when the LO moves. An actual
    // emission should remain at the same absolute frequency.
    const unsigned shifted = (int)centre + (shape.offset_khz < 0 ? -8 : 8);
    frequency_mhz = shifted;
    rx_ready = false;
    prepare_rx();
    uint32_t elapsed;
    if (spec_compute(1, 256, 8, spec_bins, &elapsed)) return;
    scout_rf_shape_t check;
    scout_rf_shape(spec_bins, &check);
    const int confirmed = (int)shifted * 1000 + check.offset_khz;
    if (!check.width_khz || abs(estimate - confirmed) > 2500) return;

    uint32_t profiles = 0;
    if (mhz >= 5725 && mhz <= 5850 && shape.width_khz >= 6250) profiles |= SCOUT_TIMING_O4;
    if (mhz >= 5650 && mhz <= 5925 && shape.width_khz >= 14000 && shape.width_khz <= 24000)
        profiles |= SCOUT_TIMING_AVATAR;

    uint8_t kind = SCOUT_RF_UNKNOWN;
    if (profiles && !fast_capture(FPV_SAMPLES, 1, &elapsed)) {
        const uint32_t match = scout_rf_timing(DUMP_BUF, FPV_SAMPLES, profiles, NULL);
        if (match == SCOUT_TIMING_O4) kind = SCOUT_RF_O4;
        if (match == SCOUT_TIMING_AVATAR) kind = SCOUT_RF_AVATAR;
    }
    if (cal_digital && kind == SCOUT_RF_UNKNOWN && scout_cal_analog_sync(mhz)) return;
    scout_rf_hit_t *best = scout.page == SCOUT_MENU ? &scout.all.digital.best : &scout.digital.best;
    const bool named = kind > SCOUT_RF_UNKNOWN, best_named = best->kind > SCOUT_RF_UNKNOWN;
    if ((named && !best_named) || (named == best_named && shape.contrast > best->contrast))
        *best = (scout_rf_hit_t){mhz, shape.width_khz, shape.contrast, kind};
}
#endif
static void scout_spectrum_step(int64_t now) {
    scout.spectrum_tick=now;
    if (!scout.spectrum_step) for(unsigned x=0;x<SCOUT_SPEC_W;x++) scout.spectrum_work[x]=INT16_MIN;
    unsigned saved_freq=frequency_mhz, saved_mode=rx_channel_mode;
    int saved_filter=rx_analog_filter;
    const bool band24 = scout.page == SCOUT_CONTROL || (scout.page == SCOUT_SPECTRUM && scout.spectrum_24) ||
                        (scout.page == SCOUT_SIGNAL_CAL && scout.cal.tool == SCOUT_CAL_CONTROL);
    const unsigned low = band24 ? SCOUT_BAND_24_LOW : SCOUT_BAND_LOW;
    const unsigned high = band24 ? SCOUT_BAND_24_HIGH : SCOUT_BAND_HIGH;
    const unsigned chunks = (high - low + SCOUT_SWEEP_KEEP - 1) / SCOUT_SWEEP_KEEP;
    unsigned centre=low+SCOUT_SWEEP_KEEP/2+scout.spectrum_step*SCOUT_SWEEP_KEEP;
    frequency_mhz=centre; rx_channel_mode=rx_bandwidth_phy_mode(40);
    rx_analog_filter=rx_bandwidth_dcap(40); rx_ready=false; prepare_rx();
    uint32_t elapsed;
    const char *err=spec_compute(1,256,4,spec_bins,&elapsed);
    if (err && scout.page == SCOUT_SIGNAL_CAL) scout.cal.baseline_error = true;
    if (!err && band24) {
        const int bandwidth = (SCOUT_BAND_24_HIGH - SCOUT_BAND_24_LOW) * 1000;
        for (int j = 32; j < 224; j++) {
            int khz = (int)centre * 1000 + (j - 128) * 40000 / 256 - SCOUT_BAND_24_LOW * 1000;
            if (khz < 0 || khz > bandwidth) continue;
            int x = (khz * (SCOUT_SPEC_W - 1) + bandwidth / 2) / bandwidth;
            if (spec_bins[j] > scout.spectrum_work[x]) scout.spectrum_work[x] = spec_bins[j];
        }
    } else if (!err) {
        scout_spectrum_add(scout.spectrum_work,centre,spec_bins);
#ifdef SCOUT_RUST_CORE
        scout_rf_probe(centre);
#endif
    }
    frequency_mhz=saved_freq; rx_channel_mode=saved_mode;
    rx_analog_filter=saved_filter; rx_ready=false; prepare_rx();
    if (++scout.spectrum_step>=chunks) {
        memcpy(scout.spectrum_plot,scout.spectrum_work,sizeof(scout.spectrum_plot));
        int32_t floor=INT32_MAX;
        for(unsigned x=0;x<SCOUT_SPEC_W;x++)
            if(scout.spectrum_plot[x]!=INT16_MIN && scout.spectrum_plot[x]<floor) floor=scout.spectrum_plot[x];
        scout.spectrum_valid=floor!=INT32_MAX;
        scout.spectrum_updated=now;
        if(scout.spectrum_valid) scout.floor_est=floor;
        scout.spectrum_step=0;
        if (scout.page == SCOUT_SIGNAL_CAL && scout.cal.tool == SCOUT_CAL_DIGITAL &&
            scout.cal.stage == SCOUT_CAL_CHECK) {
            scout_rf_finish_sweep(&scout.digital);
        } else if (scout.page == SCOUT_SCAN_PAGE || scout.page == SCOUT_MENU) {
            scout_rf_track_t *track = scout.page == SCOUT_MENU ? &scout.all.digital : &scout.digital;
            const scout_rf_hit_t observed = track->best;
            const scout_rf_hit_t before = track->shown;
            if (scout_rf_finish_sweep(track)) scout.dirty = true;
            const scout_rf_hit_t shown = track->shown;
            if (observed.kind && track->seen >= 2 && shown.kind)
                scout_log_record(SCOUT_LOG_DIGITAL + shown.kind - SCOUT_RF_UNKNOWN, shown.mhz, observed.contrast / 100, now);
            if (!observed.kind && !shown.kind && before.kind)
                scout_log_lost(SCOUT_LOG_DIGITAL + before.kind - SCOUT_RF_UNKNOWN, before.mhz, now);
        }
    }
}
static bool scout_update_spectrum(int64_t now) {
    const int64_t previous = scout.spectrum_updated;
    scout_spectrum_step(now);
    if (scout.spectrum_updated == previous) return false;
    if (scout.spectrum_valid && scout.waterfall) {
        uint8_t *row = scout.waterfall + scout.waterfall_head * SCOUT_SPEC_W;
        for (unsigned x = 0; x < SCOUT_SPEC_W; x++) {
            int32_t level = scout.spectrum_plot[x] == INT16_MIN ? 0 :
                (scout.spectrum_plot[x] - scout.floor_est) * 255 / 5000;
            row[x] = level < 0 ? 0 : level > 255 ? 255 : (uint8_t)level;
        }
        scout.waterfall_head = (scout.waterfall_head + 1) % SCOUT_WATERFALL_ROWS;
        if (scout.waterfall_count < SCOUT_WATERFALL_ROWS) scout.waterfall_count++;
    }
    return true;
}
static void scout_draw_scan_channel(bool sweeping) {
    if (scout.video_full) return;
    const int width = cyd_width();
    const uint16_t white = CYD_RGB(238,243,250), muted = CYD_RGB(145,161,182);
    const uint16_t backdrop = CYD_RGB(6,9,14);
    char frequency[24], progress[16];
    const scout_channel_t channel = scout_current_channel();
    const char *detail = scout.video_live ? (scout.video_pal ? "PAL" : "NTSC") : "Video";
    snprintf(frequency, sizeof(frequency), "%u MHz", channel.mhz);
    if (sweeping) {
        snprintf(progress, sizeof(progress), "%u/%u", scout.channel + 1, (unsigned)SCOUT_COUNT);
        detail = progress;
    }
    if (cyd_landscape()) {
        const int x = 72, pill_width = 118;
        char caption[32];
        snprintf(caption,sizeof(caption),"%s  /  %s",channel.name,detail);
        const scout_bg_label_t labels[] = {
            {x + (pill_width-cyd_label_width(frequency,CYD_FONT_UI))/2,5,frequency,white,CYD_FONT_UI},
            {x + (pill_width-cyd_label_width(caption,CYD_FONT_SMALL))/2,21,caption,muted,CYD_FONT_SMALL},
        };
        scout_pill(x,4,pill_width,32,backdrop,labels,sizeof(labels)/sizeof(labels[0]));
        return;
    }
    const int y = SCOUT_VIDEO_Y - 20;
    const scout_bg_label_t labels[] = {
        {12,y+1,channel.name,white,CYD_FONT_UI},
        {(width-cyd_label_width(frequency,CYD_FONT_UI))/2,y+1,frequency,white,CYD_FONT_UI},
        {width-12-cyd_label_width(detail,CYD_FONT_SMALL),y+3,detail,muted,CYD_FONT_SMALL},
    };
    scout_strip(y,SCOUT_VIDEO_Y,backdrop,false,labels,sizeof(labels)/sizeof(labels[0]));
}

static void scout_draw(int64_t now) {
    if (scout.video_full) {
        const uint8_t state = scout.video_live ? 2 : 1;
        if (state != scout.video_full_state) {
            cyd_fill(0,0,cyd_width(),cyd_height(),CYD_BLACK);
            if (state == 1) scout_center(0,cyd_height()/2,cyd_width(),"Waiting for video",CYD_WHITE,CYD_BLACK);
            scout_video_invalidate();
            scout.video_full_state = state;
        }
        scout.last_draw = now;
        scout.dirty = false;
        return;
    }
    const int width = cyd_width(), height = cyd_height();
    const int video_y = SCOUT_VIDEO_Y, video_h = SCOUT_VIDEO_H;
    const int button_w = width / 2 - 18, right_button = width / 2 + 6;
    const int button_y = SCOUT_BUTTON_Y;
    unsigned mode = scout.scan.mode;
    bool hit = mode == FPV_HIT, locked = mode == FPV_LOCK;
    bool sync = scout.scan.last_sync && now - scout.scan.last_sync < 1000000;
    const uint16_t bg = CYD_RGB(10,14,20);
    const uint16_t muted = CYD_RGB(145,161,182), white = CYD_RGB(238,243,250);
    const uint16_t teal = CYD_RGB(64,218,190), amber = CYD_RGB(255,192,96);
    const uint16_t button = CYD_RGB(27,38,54);
    bool recovering = locked && scout.scan.last_sync &&
        (sync ? sv_v.last_row_us && now - sv_v.last_row_us >= 1000000 :
                now - scout.scan.last_sync < sv_FRAME_HOLD_US);
    bool error = scout.errors >= 3, lost = locked && (!sync || recovering || scout.rechecking);
    uint16_t accent = error || lost ? amber : teal;
    uint16_t pill = error || lost ? CYD_RGB(57,40,23) : CYD_RGB(17,48,44);
    const char *status = scout.rechecking ? "Rechecking" : error ? "Error" : scout.comparing ? "Comparing" : hit ? "Detected" : locked ? (recovering ? "Recovering" : sync ? "Locked" : scout.scan.last_sync ? "Sync lost" : "Checking") : mode == FPV_VERIFY ? "Checking" : scout.manual ? "Manual" : "Scanning";
    bool ready = scout_wallpaper_visible() && cyd_frame_ready();
    bool animated = ready && cyd_wallpaper_update(now);
    bool refresh = scout.dirty || !scout.last_draw || animated;
    bool buffered = refresh && cyd_frame_begin();
    bool wallpaper = scout_wallpaper_visible();
    const int scale_y = SCOUT_SPEC_Y + SCOUT_SPEC_H + 1;
    const char *middle = "5.8 GHz";
    const char *label = scout.comparing ? "Comparing video" : hit || locked ? "Waiting for sync" : scout.manual ? "Monitoring channel" : "Scanning";
    const char *hint = hit || locked ? "Analog video preview" : "No confirmed video";
    char rf_detail[48];
    if (scout.digital.shown.kind && mode == FPV_SCAN && !scout.comparing && !error) {
        static const char *const labels[] = {"", "Digital signal", "Likely DJI O4", "Likely Avatar"};
        label = labels[scout.digital.shown.kind];
        snprintf(rf_detail, sizeof(rf_detail), "~%u MHz  /  %s", scout.digital.shown.mhz,
                 scout.digital.shown.kind == SCOUT_RF_UNKNOWN ? "Type unconfirmed" : "Medium confidence");
        hint = rf_detail;
    }
    const uint16_t video_bg = CYD_RGB(13,19,28);
    if (refresh) {
        const bool compact_title = locked;
        const scout_bg_label_t top[] = {
            {compact_title ? 72 : 78,compact_title ? 11 : 8,cyd_landscape() ? "" : "Video",white,
             compact_title ? CYD_FONT_SMALL : CYD_FONT_TITLE},
            {12,scale_y,"5.362",muted,CYD_FONT_SMALL},
            {(width-cyd_label_width(middle,CYD_FONT_SMALL))/2,scale_y,middle,white,CYD_FONT_SMALL},
            {width-12-cyd_label_width("5.945",CYD_FONT_SMALL),scale_y,"5.945",muted,CYD_FONT_SMALL},
        };
        scout_background(0, video_y, bg, top, sizeof(top) / sizeof(top[0]));
        if (!scout.video_live) {
            const scout_bg_label_t idle[] = {
                {(width-cyd_label_width(label,CYD_FONT_UI))/2,video_y+video_h/2-13,label,white,CYD_FONT_UI},
                {(width-cyd_label_width(hint,CYD_FONT_SMALL))/2,video_y+video_h/2+13,hint,muted,CYD_FONT_SMALL},
            };
            scout_strip(video_y, video_y + video_h, video_bg, wallpaper, idle, sizeof(idle) / sizeof(idle[0]));
        }
        scout_background(video_y + video_h, height, bg, NULL, 0);
        scout_round(8,6,58,28,7,button);
        scout_center(8,11,58,"Back",white,button);
    }
    const int pill_w = locked ? 110 : 78, pill_x = width - pill_w - 12;
    char rssi[12], lq[8], link[32], strength[20] = "";
    int status_y = 12;
    if (locked && sync && !recovering && !error && !scout.rechecking) {
        scout_signal_labels(now, rssi, lq);
        snprintf(link, sizeof(link), "%s LQ %s", status, lq);
        snprintf(strength, sizeof(strength), "RSSI %s", rssi);
        status = link;
        status_y = 6;
    }
    const scout_bg_label_t badge[] = {
        {pill_x + (pill_w - cyd_label_width(status, CYD_FONT_SMALL)) / 2,
         status_y, status, accent, CYD_FONT_SMALL},
        {pill_x + (pill_w - cyd_label_width(strength, CYD_FONT_SMALL)) / 2,
         21, strength, white, CYD_FONT_SMALL},
    };
    scout_pill(pill_x, locked ? 4 : 8, pill_w, locked ? 32 : 24, pill, badge, status_y == 6 ? 2 : 1);
    scout_draw_scan_plot();
    scout_draw_scan_channel(!scout.manual && !scout.comparing && mode == FPV_SCAN);
    if (refresh && scout_single_scan_action()) {
        scout_round(12, button_y, width - 24, SCOUT_BUTTON_H, 8, button);
        scout_center(12, button_y+7, width - 24, "Manual", white, button);
    } else if (refresh) {
        scout_round(12, button_y, button_w, SCOUT_BUTTON_H, 8, button);
        scout_round(right_button, button_y, button_w, SCOUT_BUTTON_H, 8, button);
        scout_center(12, button_y+7, button_w, "Auto scan", white, button);
        if (scout.manual) {
            const int arrow = SCOUT_ARROW_W(cyd_landscape());
            scout_center(right_button,button_y+7,button_w,"Channel",white,button);
            scout_center(right_button,button_y+7,arrow,"<",teal,button);
            scout_center(right_button+button_w-arrow,button_y+7,arrow,">",teal,button);
        } else {
            scout_center(right_button, button_y+7, button_w,"Continue",white,button);
        }
    }
    if (buffered) cyd_frame_end(scout.video_live ? video_y : 0, scout.video_live ? video_y + video_h : 0);
    scout.last_draw = now;
    scout.dirty = false;
}

static void scout_draw_tuner(void) {
    static const char *const digit_labels[3][4] = {
        {"1","2","3","Clear"}, {"4","5","6","Del"}, {"7","8","9","0"},
    };
    const int width = cyd_width(), height = cyd_height();
    const uint16_t bg = CYD_RGB(10,14,20), button = CYD_RGB(27,38,54);
    const uint16_t selected = CYD_RGB(17,58,53), muted = CYD_RGB(145,161,182);
    const uint16_t white = CYD_RGB(238,243,250), teal = CYD_RGB(64,218,190);
    const bool direct = scout.tuner_direct, valid = scout_tuner_valid();
    const int pitch = SCOUT_TUNER_PITCH, key_h = SCOUT_TUNER_KEY_H;
    const int key_y = SCOUT_TUNER_KEY_Y, bands = SCOUT_COUNT / SCOUT_CHANNELS_PER_BAND;
    char value[24];
    if (!direct) {
        const scout_channel_t channel = scout_channels[scout.tuner_channel];
        snprintf(value, sizeof(value), "%s  %u MHz", channel.name, channel.mhz);
    } else if (scout.tuner_mhz) {
        snprintf(value, sizeof(value), "%u MHz", (unsigned)scout.tuner_mhz);
    } else snprintf(value, sizeof(value), "%u-%u MHz", SCOUT_BAND_LOW, SCOUT_BAND_HIGH);
    const scout_bg_label_t labels[] = {
        {78,8,"Tune channel",white,CYD_FONT_TITLE},
        {(width-cyd_label_width(value,CYD_FONT_UI))/2,cyd_landscape()?80:92,value,
         valid ? white : muted,CYD_FONT_UI},
    };
    const bool buffered = cyd_frame_begin();
    scout_background(0,height,bg,labels,sizeof(labels)/sizeof(labels[0]));
    scout_round(8,6,58,28,7,button);
    scout_center(8,11,58,"Back",white,button);
    for (int tab = 0; tab < 2; tab++) {
        const int x = scout_tuner_edge(tab,2), w = scout_tuner_edge(tab+1,2) - x - 6;
        const bool active = tab == direct;
        const uint16_t color = active ? selected : button;
        scout_round(x,SCOUT_TUNER_TAB_Y,w,SCOUT_BUTTON_H,8,color);
        scout_center(x,SCOUT_TUNER_TAB_Y+7,w,tab ? "Frequency" : "Band / Ch",active ? teal : white,color);
    }
    for (int row = 0; row < 3; row++) {
        const int columns = !direct && row == 0 ? bands : 4;
        for (int col = 0; col < columns; col++) {
            const int x = scout_tuner_edge(col,columns), w = scout_tuner_edge(col+1,columns) - x - 6;
            const int y = key_y + row*pitch;
            char key[2] = {'\0','\0'};
            bool active = false;
            if (!direct && row == 0) {
                key[0] = scout_channels[col * SCOUT_CHANNELS_PER_BAND].name[0];
                active = col == scout.tuner_channel / SCOUT_CHANNELS_PER_BAND;
            } else if (!direct) {
                const int number = (row - 1)*4 + col;
                key[0] = '1' + number;
                active = number == scout.tuner_channel % SCOUT_CHANNELS_PER_BAND;
            }
            const uint16_t color = active ? selected : button;
            scout_round(x,y,w,key_h,7,color);
            scout_center(x,y+(key_h-18)/2,w,direct ? digit_labels[row][col] : key,active ? teal : white,color);
        }
    }
    scout_round(12,SCOUT_BUTTON_Y,width-24,SCOUT_BUTTON_H,8,valid ? selected : button);
    scout_center(12,SCOUT_BUTTON_Y+7,width-24,"Tune",valid ? teal : muted,valid ? selected : button);
    if (buffered) cyd_frame_end(0,0);
    scout.dirty = false;
}

static void scout_log_time(char text[32], uint32_t boot, uint32_t seconds) {
    if (!boot) { strcpy(text,"Unknown"); return; }
    snprintf(text,32,"Boot %lu / %02lu:%02lu:%02lu",(unsigned long)boot,
             (unsigned long)(seconds / 3600),(unsigned long)(seconds / 60 % 60),
             (unsigned long)(seconds % 60));
}

static void scout_draw_signal_log(int64_t now) {
    const int width = cyd_width(), height = cyd_height();
    const unsigned rows = SCOUT_LOG_ROWS(height), start = scout.log_page * rows;
    const uint16_t bg = CYD_RGB(10,14,20), card = CYD_RGB(17,24,35), button = CYD_RGB(27,38,54);
    const uint16_t white = CYD_RGB(238,243,250), muted = CYD_RGB(145,161,182);
    const bool detail = scout.log_selection >= 0;
    const bool buffered = cyd_frame_begin();
    const scout_bg_label_t title = {78,8,detail?"Signal Details":"Signal Log",white,CYD_FONT_TITLE};
    scout_background(0,height,bg,&title,1);
    scout_round(8,6,58,28,7,button);
    scout_center(8,11,58,"Back",white,button);
    if (detail) {
        const scout_log_entry_t *e = &scout_log.entries[scout.log_selection];
        char frequency[36], strength[32], logged[32], seen[32], lost[32];
        const char *band = scout_log_band(e->kind,e->mhz);
        if (e->mhz) snprintf(frequency,sizeof(frequency),"%u MHz / %s",e->mhz,band);
        else snprintf(frequency,sizeof(frequency),"%s",band);
        if (e->strength == SCOUT_LOG_NO_LEVEL) strcpy(strength,"Unavailable");
        else snprintf(strength,sizeof(strength),"%+ld %s",(long)e->strength,scout_log_strength_unit(e->kind));
        scout_log_time(logged,e->first_run,e->first_seconds);
        scout_log_time(seen,e->seen_run,e->seen_seconds);
        scout_log_time(lost,e->lost_run,e->lost_seconds);
        const char *const labels[] = {"Frequency", "Strength", "Logged", "Last seen", "Last lost"};
        const char *const values[] = {frequency,strength,logged,seen,lost};
        cyd_label(16,47,scout_log_names[e->kind],white,bg,CYD_FONT_UI);
        for (unsigned i=0; i<5; ++i) {
            const int y = 82 + i * 27;
            cyd_label(16,y,labels[i],muted,bg,CYD_FONT_SMALL);
            cyd_label(84,y-1,values[i],white,bg,CYD_FONT_SMALL);
        }
    } else {
        if (scout.log_clear_confirm) {
            cyd_label(16,74,"Clear the active log?",white,bg,CYD_FONT_UI);
        } else if (!scout_log.count) {
            cyd_label(16,70,"No signals recorded",white,bg,CYD_FONT_UI);
            cyd_label(16,96,"Run a scanner to fill the log",muted,bg,CYD_FONT_SMALL);
        } else {
            if (scout_log.count > rows) {
                scout_round(12,37,70,23,6,button);
                scout_round(width-82,37,70,23,6,button);
                scout_center(12,40,70,"Newer",start?white:muted,button);
                scout_center(width-82,40,70,"Older",start+rows<scout_log.count?white:muted,button);
            }
            for (unsigned i = 0; i < rows && start + i < scout_log.count; ++i) {
                const scout_log_entry_t *e = &scout_log.entries[start + i];
                const int y = SCOUT_LOG_ROW_Y + i * SCOUT_LOG_ROW_H;
                const unsigned age = (uint32_t)(now / 1000000) - e->seen_seconds;
                char frequency[24], last_seen[22];
                if (e->mhz) snprintf(frequency,sizeof(frequency),"%u MHz",e->mhz);
                else snprintf(frequency,sizeof(frequency),"%s",scout_log_band(e->kind,0));
                if (e->seen_run != scout_log.run) snprintf(last_seen,sizeof(last_seen),"Boot %lu",(unsigned long)e->seen_run);
                else if (age < 2) strcpy(last_seen,"Just now");
                else if (age < 60) snprintf(last_seen,sizeof(last_seen),"%u s ago",age);
                else if (age < 3600) snprintf(last_seen,sizeof(last_seen),"%u min ago",age / 60);
                else snprintf(last_seen,sizeof(last_seen),"%u h ago",age / 3600);
                scout_round(12,y,width-24,32,6,card);
                cyd_label(20,y+1,scout_log_names[e->kind],white,card,CYD_FONT_UI);
                cyd_label(20,y+17,frequency,muted,card,CYD_FONT_SMALL);
                cyd_label(width-20-cyd_label_width(last_seen,CYD_FONT_SMALL),y+17,last_seen,muted,card,CYD_FONT_SMALL);
            }
        }
        const char *status = scout.log_status[0] ? scout.log_status :
            scout_log_fault ? "Log storage unavailable" :
            scout_log.count == SCOUT_LOG_CAPACITY ? "Log full. Save or clear." : "";
        if (!scout.log_clear_confirm && status[0]) cyd_label(16,height-54,status,muted,bg,CYD_FONT_SMALL);
        const int button_w = width / 2 - 18, right = width / 2 + 6;
        scout_round(12,SCOUT_BUTTON_Y,button_w,SCOUT_BUTTON_H,8,button);
        scout_round(right,SCOUT_BUTTON_Y,button_w,SCOUT_BUTTON_H,8,button);
        const uint16_t button_fg = scout_log.count ? white : muted;
        scout_center(12,SCOUT_BUTTON_Y+7,button_w,scout.log_clear_confirm?"Cancel":"Save to SD",button_fg,button);
        scout_center(right,SCOUT_BUTTON_Y+7,button_w,scout.log_clear_confirm?"Clear log":"Clear",button_fg,button);
    }
    if (buffered) cyd_frame_end(0,0);
    scout.last_draw = now;
    scout.dirty = false;
}
static void scout_draw_settings(void) {
    const bool landscape = cyd_landscape();
    const int width = cyd_width(), card_w = width - 32;
    const int half_w = (card_w - 12) / 2, right_x = width / 2 + 6;
    const int utility_w = landscape ? half_w : card_w;
    const int test_x = landscape ? right_x : 16;
    const int test_y = landscape ? SCOUT_CALIBRATE_Y : SCOUT_TEST_LED_PORTRAIT_Y;
    const uint16_t bg=CYD_RGB(10,14,20), button=CYD_RGB(27,38,54);
    const uint16_t white=CYD_RGB(238,243,250), muted=CYD_RGB(145,161,182);
    bool buffered = scout_wallpaper_visible() && cyd_frame_begin();
    const scout_bg_label_t labels[] = {
        {78,8,"Settings",white,CYD_FONT_TITLE},
        {16,cyd_height()-13,cyd_wallpaper_status(),muted,CYD_FONT_SMALL},
    };
    scout_background(0,cyd_height(),bg,labels,sizeof(labels) / sizeof(labels[0]));
    scout_round(8,6,58,28,7,button);
    scout_center(8,11,58,"Back",white,button);
    scout_round(16,SCOUT_RESCAN_Y,utility_w,SCOUT_SETTING_H,9,button);
    cyd_label(28,SCOUT_RESCAN_Y+9,"Auto rescan",muted,button,CYD_FONT_UI);
    const char *rescan = scout.auto_rescan ? "On" : "Off";
    cyd_label(16+utility_w-12-cyd_label_width(rescan,CYD_FONT_UI),SCOUT_RESCAN_Y+9,rescan,white,button,CYD_FONT_UI);
    const int start_x = landscape ? right_x : 16, start_y = SCOUT_START_PAGE_Y(landscape);
    const char *start = scout_start_pages[scout.start_page].label;
    scout_round(start_x,start_y,utility_w,SCOUT_SETTING_H,9,button);
    cyd_label(start_x+12,start_y+(landscape ? 3 : 9),"Start page",muted,button,
              landscape ? CYD_FONT_SMALL : CYD_FONT_UI);
    const int start_value_x = landscape ? start_x+12 : width-33-cyd_label_width(start,CYD_FONT_UI);
    cyd_label(start_value_x,start_y+(landscape ? 16 : 9),start,white,button,CYD_FONT_UI);
    scout_round(16,SCOUT_ORIENTATION_Y,card_w,SCOUT_SETTING_H,9,button);
    cyd_label(28,SCOUT_ORIENTATION_Y+9,"Orientation",muted,button,CYD_FONT_UI);
    const char *orientation = landscape ? "Landscape" : "Portrait";
    cyd_label(width-33-cyd_label_width(orientation,CYD_FONT_UI),SCOUT_ORIENTATION_Y+9,
              orientation,white,button,CYD_FONT_UI);
    scout_round(16,SCOUT_SIGNAL_CAL_Y,card_w,SCOUT_SETTING_H,9,button);
    scout_center(16,SCOUT_SIGNAL_CAL_Y+7,card_w,"Signal Calibration",white,button);
    scout_round(16,SCOUT_WALLPAPER_Y,half_w,SCOUT_SETTING_H,9,button);
    scout_round(right_x,SCOUT_WALLPAPER_Y,half_w,SCOUT_SETTING_H,9,button);
    const char *const modes[] = {"Main", "All", "Off"};
    const char *mode = modes[cyd_wallpaper_mode()];
    cyd_label(16+(half_w-cyd_label_width("Wallpaper",CYD_FONT_SMALL))/2,SCOUT_WALLPAPER_Y+2,
              "Wallpaper",muted,button,CYD_FONT_SMALL);
    cyd_label(16+(half_w-cyd_label_width(mode,CYD_FONT_SMALL))/2,SCOUT_WALLPAPER_Y+16,
              mode,white,button,CYD_FONT_SMALL);
    scout_center(right_x,SCOUT_WALLPAPER_Y+7,half_w,"Next image",white,button);
    scout_round(16,SCOUT_CALIBRATE_Y,utility_w,SCOUT_SETTING_H,9,button);
    scout_center(16,SCOUT_CALIBRATE_Y+9,utility_w,"Calibrate touch",white,button);
    scout_round(test_x,test_y,utility_w,SCOUT_SETTING_H,9,button);
    scout_center(test_x,test_y+9,utility_w,"Test LED",white,button);
    if (buffered) cyd_frame_end(0,0);
    scout.dirty=false;
}

static void scout_cal_result_lines(unsigned tool, const scout_cal_reference_t *r,
                                   char primary[48], char secondary[48]) {
    switch (tool) {
    case SCOUT_CAL_ANALOG:
        snprintf(primary,48,"%u MHz   %s",r->mhz,r->signal == 1 ? "PAL" : "NTSC");
        snprintf(secondary,48,"LQ %u%%   Picture %d%%",r->quality,r->level);
        break;
    case SCOUT_CAL_DIGITAL:
        snprintf(primary,48,"%s   %u MHz",r->signal == SCOUT_RF_O4 ? "Likely DJI O4" :
                 r->signal == SCOUT_RF_AVATAR ? "Likely Avatar" : "Unclassified RF",r->mhz);
        snprintf(secondary,48,"Contrast %d dB",r->level / 100);
        break;
    case SCOUT_CAL_CONTROL:
        snprintf(primary,48,"%s",r->signal == 1 ? "Likely ELRS" : "DJI-like activity");
        snprintf(secondary,48,"RSSI %d dBFS   LQ %u%%",r->level,r->quality);
        break;
    case SCOUT_CAL_RID:
        if (r->signal & 1) snprintf(primary,48,"Wi-Fi received   %d dBm",r->level);
        else snprintf(primary,48,"Wi-Fi not seen");
        if (r->signal & 2) snprintf(secondary,48,"Bluetooth received   %d dBm",r->other_level);
        else snprintf(secondary,48,"Bluetooth not seen");
        break;
    }
}

static void scout_draw_calibration(int64_t now) {
    const scout_signal_cal_t *cal = &scout.cal;
    const unsigned tool = cal->tool, stage = cal->stage;
    const int width = cyd_width(), height = cyd_height(), card_w = width - 32;
    const uint16_t bg=CYD_RGB(10,14,20), button=CYD_RGB(27,38,54);
    const uint16_t white=CYD_RGB(238,243,250), muted=CYD_RGB(145,161,182), teal=CYD_RGB(64,218,190);
    const bool buffered = cyd_frame_begin();
    const scout_bg_label_t title[] = {{77,8,scout_cal_names[tool],white,CYD_FONT_TITLE}};
    scout_background(0,height,bg,title,1);
    scout_round(8,6,58,28,7,button);
    scout_center(8,11,58,"Back",white,button);

    if (tool == SCOUT_CAL_MENU) {
        for (unsigned i = SCOUT_CAL_ANALOG; i <= SCOUT_CAL_RID; ++i) {
            const int y = SCOUT_CAL_ROW_Y + (int)(i - SCOUT_CAL_ANALOG) * SCOUT_CAL_ROW_PITCH;
            scout_round(16,y,card_w,SCOUT_CAL_ROW_H,8,button);
            scout_center(16,y+7,card_w,scout_cal_names[i],white,button);
        }
    } else {
        const char *heading = "", *first = "", *second = "", *action = NULL;
        char details[48] = "", measurement[48] = "", baseline[48] = "";
        if (cal->baseline_ready) snprintf(baseline,sizeof(baseline),"Quiet floor %+d dB rel.",cal->result.floor / 100);
        switch (stage) {
        case SCOUT_CAL_SETUP:
            if (tool == SCOUT_CAL_RID) {
                heading = "Reference broadcast";
                first = "Power on your Remote ID beacon.";
                second = "Listen for Wi-Fi and Bluetooth.";
                action = "Listen";
            } else {
                heading = "Quiet baseline";
                first = tool == SCOUT_CAL_CONTROL ? "Switch off your test controller." : "Switch off your video transmitter.";
                second = "Keep the receiver in place.";
                action = "Measure baseline";
            }
            break;
        case SCOUT_CAL_BASELINE:
            heading = "Measuring quiet band";
            snprintf(details,sizeof(details),"Sweep %u of 3",cal->baseline_passes+1);
            first = details;
            break;
        case SCOUT_CAL_READY:
            heading = "Reference signal";
            first = tool == SCOUT_CAL_CONTROL ? "Power on your test controller." : "Power on your test video link.";
            second = tool == SCOUT_CAL_DIGITAL ? "Wait for the link to connect." : "Keep the receiver in place.";
            action = "Check signal";
            break;
        case SCOUT_CAL_CHECK:
            heading = "Checking reference";
            if (tool == SCOUT_CAL_ANALOG && scout.scan.mode == FPV_LOCK) {
                const int lq = scout_lq();
                if (lq >= 0) snprintf(details,sizeof(details),"%u MHz   LQ %d%%",scout_current_channel().mhz,lq);
                else snprintf(details,sizeof(details),"%u MHz   Measuring sync",scout_current_channel().mhz);
                first = details;
            } else if (tool == SCOUT_CAL_RID) {
                scout_cal_result_lines(tool,&cal->result,details,measurement);
                first = details;
                second = measurement;
                if (cal->result.signal) action = "Use results";
            } else {
                first = "Listening for the reference signal.";
            }
            break;
        case SCOUT_CAL_REVIEW:
        case SCOUT_CAL_SAVED:
            heading = stage == SCOUT_CAL_SAVED ? "Reference saved" :
                      cal->save_failed ? "Save failed. Try again." : "Review reference";
            scout_cal_result_lines(tool,&cal->result,details,measurement);
            first = details;
            second = measurement;
            action = stage == SCOUT_CAL_SAVED ? "Run again" : "Save reference";
            break;
        case SCOUT_CAL_FAILED:
            heading = tool == SCOUT_CAL_RID || cal->baseline_ready ?
                      "Reference unconfirmed" : "Baseline unavailable";
            first = tool == SCOUT_CAL_RID ? "Check that your beacon is active." :
                    cal->baseline_ready ? "Check the link and try again." : "Try again with your source off.";
            action = "Try again";
            break;
        }
        char saved_primary[48] = "", saved_secondary[48] = "", saved_floor[42] = "";
        if (cal->saved_valid && stage != SCOUT_CAL_SAVED) {
            scout_cal_result_lines(tool,&cal->saved,saved_primary,saved_secondary);
            if (tool != SCOUT_CAL_RID) snprintf(saved_floor,sizeof(saved_floor),"Saved quiet floor %+d dB rel.",cal->saved.floor / 100);
        }
        const scout_bg_label_t labels[] = {
            {16,46,heading,teal,CYD_FONT_UI},
            {16,69,first,white,CYD_FONT_SMALL},
            {16,88,second,white,CYD_FONT_SMALL},
            {16,109,baseline,muted,CYD_FONT_SMALL},
            {16,134,saved_primary[0] ? "Saved reference" : "",teal,CYD_FONT_SMALL},
            {16,151,saved_primary,muted,CYD_FONT_SMALL},
            {16,168,saved_secondary,muted,CYD_FONT_SMALL},
            {16,185,saved_floor,muted,CYD_FONT_SMALL},
        };
        scout_background(41,SCOUT_BUTTON_Y,bg,labels,sizeof(labels)/sizeof(labels[0]));
        if (action) {
            scout_round(16,SCOUT_BUTTON_Y,card_w,SCOUT_BUTTON_H,8,button);
            scout_center(16,SCOUT_BUTTON_Y+7,card_w,action,white,button);
        }
    }
    if (buffered) cyd_frame_end(0,0);
    scout.last_draw = now;
    scout.dirty = false;
}

static void scout_draw_remote_id(int64_t now) {
    const int width = cyd_width(), height = cyd_height();
    const int top = 64, pitch = cyd_landscape() ? 42 : 54;
    const uint16_t bg = CYD_RGB(10,14,20), button = CYD_RGB(27,38,54), card = CYD_RGB(17,24,35);
    const uint16_t white = CYD_RGB(238,243,250), muted = CYD_RGB(145,161,182);
    const bool buffered = scout.dirty && cyd_frame_begin();
    if (scout.dirty) {
        const scout_bg_label_t title[] = {{77,8,"RemoteID Check",white,CYD_FONT_TITLE}};
        scout_background(0,41,bg,title,1);
        scout_round(8,6,58,28,7,button);
        scout_center(8,11,58,"Back",white,button);
    }
    char status[40];
    if (scout_rid && scout_rid->ble_window)
        snprintf(status,sizeof(status),"Scanning Bluetooth");
    else if (scout_rid)
        snprintf(status,sizeof(status),scout_rid->ble_enabled ? "Scanning Wi-Fi ch %u" : "Wi-Fi ch %u / BLE unavailable",
                 scout_rid->channel);
    else
        snprintf(status,sizeof(status),"Radio unavailable");
    const scout_bg_label_t sub[] = {{16,44,status,muted,CYD_FONT_SMALL}};
    scout_background(41,top,bg,sub,1);
    unsigned shown = 0;
    for (unsigned i = 0; scout_rid && i < SCOUT_RID_CAPACITY; ++i) {
        const int64_t seen = scout_rid->aircraft[i].seen;
        if (!seen || now - seen >= SCOUT_RID_EXPIRY_US) continue;
        const scout_rid_packet_t *p = &scout_rid->aircraft[i].packet;
        const int y = top + (int)shown++ * pitch;
        char details[48];
        const char *transport = p->transport == 3 ? "Bluetooth" : p->transport == 2 ? "Wi-Fi NAN" : "Wi-Fi Beacon";
        snprintf(details,sizeof(details),"%s  /  %d dBm  /  %u s",
                 transport,p->rssi,(unsigned)((now-seen)/1000000));
        const scout_bg_label_t lines[] = {
            {20,y+3,p->id[0] ? p->id : "Waiting for aircraft ID",white,CYD_FONT_UI},
            {20,y+22,details,muted,CYD_FONT_SMALL},
        };
        scout_pill(12,y,width-24,pitch-4,card,lines,2);
        scout_background(y+pitch-4,y+pitch,bg,NULL,0);
    }
    const int clear_from = top + (int)shown * pitch;
    const scout_bg_label_t idle[] = {{16,top+11,"No Remote ID broadcasts",muted,CYD_FONT_UI}};
    scout_background(clear_from,height,bg,idle,shown || !scout_rid ? 0 : 1);
    if (buffered) cyd_frame_end(0,0);
    scout.last_draw = now;
    scout.dirty = false;
}

static void scout_draw_all(int64_t now) {
    const bool landscape = cyd_landscape();
    const int width = cyd_width(), height = cyd_height();
    const uint16_t bg = CYD_RGB(10,14,20), button = CYD_RGB(27,38,54), card = CYD_RGB(17,24,35);
    const uint16_t white = CYD_RGB(238,243,250), muted = CYD_RGB(145,161,182), teal = CYD_RGB(64,218,190);
    const char *const names[] = {"Analog Video", "Digital Video", "Control Links", "Remote ID"};
    const char *const phases[] = {"Sweeping analog video", "Checking digital activity",
                                  "Sweeping control links", "Listening for Remote ID"};
    const scout_bg_label_t heading[] = {
        {16,9,"FPV Scout",white,CYD_FONT_TITLE},
        {width-16-cyd_label_width(SCOUT_VERSION_LABEL,CYD_FONT_SMALL),14,SCOUT_VERSION_LABEL,muted,CYD_FONT_SMALL},
        {16,35,phases[scout.all.phase],muted,CYD_FONT_SMALL},
    };
    char analog[24] = "", digital[40] = "", remote[28] = "";
    const scout_rf_hit_t rf = scout.all.digital.shown;
    if (scout.all.analog_at && now - scout.all.analog_at < SCOUT_ALL_ANALOG_AGE_US) {
        const scout_channel_t ch = scout_channels[scout.all.analog_channel];
        snprintf(analog,sizeof(analog),"%s / %u MHz",ch.name,ch.mhz);
    }
    if (rf.kind && rf.mhz) {
        const char *label = rf.kind == SCOUT_RF_O4 ? "Likely DJI O4" :
                            rf.kind == SCOUT_RF_AVATAR ? "Likely Avatar" : "Digital signal";
        snprintf(digital,sizeof(digital),"%s / %u MHz",label,rf.mhz);
    }
    const bool elrs = scout.control.elrs_hops >= 2 && now - scout.control.elrs_at < 8000000;
    const bool dji = scout.control.dji_matches >= 2 && now - scout.control.dji_at < 8000000;
    const unsigned remote_count = scout_rid_recent(now);
    if (remote_count) snprintf(remote,sizeof(remote),"%u recent %s",remote_count,remote_count == 1 ? "report" : "reports");
    const char *const results[] = {analog, digital,
        elrs && dji ? "ELRS and DJI-like activity" : elrs ? "Likely ELRS" : dji ? "DJI-like activity" : "",
        remote};
    const bool buffered = cyd_frame_begin();
    scout_background(0,height,bg,heading,sizeof(heading)/sizeof(heading[0]));
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
        const int y = SCOUT_ALL_ROW_Y + (int)i * SCOUT_ALL_ROW_PITCH(landscape);
        const bool detected = results[i][0] != '\0';
        const scout_bg_label_t labels[] = {
            {28,y+2,names[i],white,CYD_FONT_UI},
            {28,y+(landscape ? 17 : 23),detected ? results[i] : "No recent detection",detected ? teal : muted,CYD_FONT_SMALL},
            {width-38,y+(landscape ? 7 : 13),">",white,CYD_FONT_UI},
        };
        scout_pill(16,y,width-32,SCOUT_ALL_ROW_H(landscape),card,labels,sizeof(labels)/sizeof(labels[0]));
    }
    const char *const tools[] = {"Spectrum", "Settings", "Signal Log", "Composite"};
    const int w = width / 2 - 22, h = SCOUT_MENU_TOOL_H(landscape);
    for (unsigned i = 0; i < sizeof(tools)/sizeof(tools[0]); ++i) {
        const int x = i % 2 ? width / 2 + 6 : 16;
        const int y = SCOUT_MENU_TOOL_Y(landscape) + (int)(i / 2) * SCOUT_MENU_TOOL_PITCH(landscape);
        scout_round(x,y,w,h,6,button);
        const int text_x = x + (w-cyd_label_width(tools[i],CYD_FONT_SMALL))/2;
        cyd_label(text_x,y+(h-12)/2,tools[i],white,button,CYD_FONT_SMALL);
    }
    if (buffered) cyd_frame_end(0,0);
    scout.last_draw = now;
    scout.dirty = false;
}

static void scout_draw_control_card(int y, int height, const char *name, const char *status,
                                    bool active, bool identified, const scout_control_reading_t *signal) {
    const int width = cyd_width();
    const uint16_t card=CYD_RGB(17,24,35), white=CYD_RGB(238,243,250);
    const uint16_t muted=CYD_RGB(145,161,182), teal=CYD_RGB(64,218,190);
    char prob[16]="Prob --", rssi[18]="RSSI --", lq[14]="LQ --";
    if (active) {
        snprintf(prob,sizeof(prob),"Prob %u%%",scout_control_probability(signal,scout.control.passes));
        snprintf(rssi,sizeof(rssi),"RSSI %ddB",signal->dbfs);
        snprintf(lq,sizeof(lq),"LQ %u%%",signal->quality);
    }
    const int metric_y = y + height - 22;
    const scout_bg_label_t labels[] = {
        {22,y+7,name,white,CYD_FONT_UI},
        {width-22-cyd_label_width(status,CYD_FONT_SMALL),y+9,status,identified?teal:muted,CYD_FONT_SMALL},
        {22,metric_y,prob,muted,CYD_FONT_SMALL},
        {(width-cyd_label_width(rssi,CYD_FONT_SMALL))/2,metric_y,rssi,muted,CYD_FONT_SMALL},
        {width-22-cyd_label_width(lq,CYD_FONT_SMALL),metric_y,lq,muted,CYD_FONT_SMALL},
    };
    scout_pill(12,y,width-24,height,card,labels,sizeof(labels)/sizeof(labels[0]));
}

static void scout_draw_control(bool waterfall_changed) {
    const int width = cyd_width(), height = cyd_height();
    const bool landscape = cyd_landscape();
    const uint16_t bg = CYD_RGB(10,14,20), button = CYD_RGB(27,38,54);
    const uint16_t white = CYD_RGB(238,243,250), muted = CYD_RGB(145,161,182);
    const scout_control_t *c = &scout.control;
    const int top = landscape ? 40 : 46, card_h = landscape ? 54 : 62;
    const int bottom = height - card_h - 8;
    const int scale_y = top + card_h + 6, waterfall_y = scale_y + 17;
    const bool full = scout.dirty, buffered = full && cyd_frame_begin();
    if (full) {
        const char *plot_title = "2.4 GHz waterfall";
        const scout_bg_label_t labels[] = {
            {78,8,"Control Links",white,CYD_FONT_TITLE},
            {12,scale_y,"2400",muted,CYD_FONT_SMALL},
            {(width-cyd_label_width(plot_title,CYD_FONT_SMALL))/2,scale_y,plot_title,white,CYD_FONT_SMALL},
            {width-12-cyd_label_width("2490",CYD_FONT_SMALL),scale_y,"2490",muted,CYD_FONT_SMALL},
        };
        scout_background(0,height,bg,labels,sizeof(labels)/sizeof(labels[0]));
        scout_round(8,6,58,28,7,button);
        scout_center(8,11,58,"Back",white,button);
    }
    char elrs_text[24]="Scanning", dji_text[24]="Scanning";
    if (c->elrs_hops == 1) snprintf(elrs_text,sizeof(elrs_text),"Checking SF%u",c->elrs_sf);
    if (c->elrs_hops >= 2) snprintf(elrs_text,sizeof(elrs_text),"Likely ELRS SF%u",c->elrs_sf);
    if (c->dji_matches == 1) snprintf(dji_text,sizeof(dji_text),"Checking");
    if (c->dji_matches >= 2) snprintf(dji_text,sizeof(dji_text),"Likely %u MHz",c->dji_mhz);
    scout_draw_control_card(top,card_h,"ELRS LoRa",elrs_text,c->elrs_hops != 0,c->elrs_hops >= 2,&c->elrs);
    if (full || waterfall_changed) scout_waterfall_draw(waterfall_y,bottom-waterfall_y-8,NULL);
    scout_draw_control_card(bottom,card_h,"DJI OcuSync",dji_text,c->dji_matches != 0,c->dji_matches >= 2,&c->dji);
    if (buffered) cyd_frame_end(0,0);
    scout.dirty = false;
}

static void scout_draw_spectrum_page(int64_t now) {
    const bool landscape = cyd_landscape(), band24 = scout.spectrum_24;
    const int width = cyd_width(), height = cyd_height(), plot_h = landscape ? 42 : 60;
    const int axes = SCOUT_SPEC_Y + plot_h + 4, waterfall_y = axes + 31;
    const int waterfall_h = SCOUT_BUTTON_Y - waterfall_y - 8;
    const int button_w = width / 2 - 18;
    const uint16_t bg = CYD_RGB(10,14,20), button = CYD_RGB(27,38,54);
    const uint16_t white = CYD_RGB(238,243,250), muted = CYD_RGB(145,161,182);
    const uint16_t teal = CYD_RGB(64,218,190), pill = CYD_RGB(17,48,44);
    const char *low = band24 ? "2.400" : "5.362";
    const char *middle = band24 ? "2.445 GHz" : "5.8 GHz";
    const char *high = band24 ? "2.490" : "5.945";
    char peak[32] = "";
    if (scout.spectrum_valid) {
        unsigned bin = 0;
        for (unsigned i = 1; i < SCOUT_SPEC_W; i++)
            if (scout.spectrum_plot[i] > scout.spectrum_plot[bin]) bin = i;
        const unsigned midpoint = SCOUT_SPEC_W / 2;
        const unsigned mhz = band24 ? SCOUT_BAND_24_LOW + bin * (SCOUT_BAND_24_HIGH - SCOUT_BAND_24_LOW) / (SCOUT_SPEC_W - 1) :
            bin <= midpoint ? SCOUT_BAND_LOW + bin * (SCOUT_BAND_MIDDLE - SCOUT_BAND_LOW) / midpoint :
            SCOUT_BAND_MIDDLE + (bin - midpoint) * (SCOUT_BAND_HIGH - SCOUT_BAND_MIDDLE) / (SCOUT_SPEC_W - 1 - midpoint);
        snprintf(peak, sizeof(peak), "Peak %u MHz", mhz);
    }

    const bool refresh = scout.dirty || !scout.last_draw;
    const bool buffered = refresh && cyd_frame_begin();
    if (refresh) {
        const scout_bg_label_t labels[] = {
            {78,8,"Spectrum",white,CYD_FONT_TITLE},
            {SCOUT_SPEC_X,axes,low,muted,CYD_FONT_SMALL},
            {(width-cyd_label_width(middle,CYD_FONT_SMALL))/2,axes,middle,white,CYD_FONT_SMALL},
            {width-SCOUT_SPEC_X-cyd_label_width(high,CYD_FONT_SMALL),axes,high,muted,CYD_FONT_SMALL},
        };
        scout_background(0,height,bg,labels,sizeof(labels)/sizeof(labels[0]));
        scout_round(8,6,58,28,7,button);
        scout_center(8,11,58,"Back",white,button);
        scout_round(width-72,8,60,24,8,pill);
        const char *state = scout.spectrum_held ? "Hold" : "Live";
        cyd_label(width-72+(60-cyd_label_width(state,CYD_FONT_SMALL))/2,12,state,teal,pill,CYD_FONT_SMALL);
        scout_round(12,SCOUT_BUTTON_Y,button_w,SCOUT_BUTTON_H,8,button);
        scout_round(width/2+6,SCOUT_BUTTON_Y,button_w,SCOUT_BUTTON_H,8,button);
        scout_center(12,SCOUT_BUTTON_Y+7,button_w,scout.spectrum_held ? "Resume" : "Hold",white,button);
        scout_center(width/2+6,SCOUT_BUTTON_Y+7,button_w,band24 ? "Use 5.8 GHz" : "Use 2.4 GHz",white,button);
    }
    const scout_bg_label_t readings[] = {
        {SCOUT_SPEC_X,axes+15,"Waterfall",muted,CYD_FONT_SMALL},
        {width-SCOUT_SPEC_X-cyd_label_width(peak,CYD_FONT_SMALL),axes+15,peak,teal,CYD_FONT_SMALL},
    };
    scout_spectrum_draw(plot_h, NULL);
    scout_background(axes+15,waterfall_y,bg,readings,sizeof(readings)/sizeof(readings[0]));
    scout_waterfall_draw(waterfall_y, waterfall_h, NULL);
    if (buffered) cyd_frame_end(0,0);
    scout.last_draw = now;
    scout.dirty = false;
}

static void scout_draw_bench(int64_t now) {
    const uint16_t bg=CYD_RGB(10,14,20), button=CYD_RGB(27,38,54);
    const uint16_t white=CYD_RGB(238,243,250), muted=CYD_RGB(145,161,182);
    const int width = cyd_width(), button_y = SCOUT_BUTTON_Y;
    const int button_w = width / 2 - 18, right_button = width / 2 + 6;
    const int view_y = 88, view_h = cyd_landscape() ? button_y - view_y - 4 : 180;
    bool buffered = scout.dirty && scout_wallpaper_visible() && cyd_frame_begin();
    if (scout.dirty) {
        const scout_bg_label_t title = {78,8,"Composite",white,CYD_FONT_TITLE};
        scout_background(0,scout_bench.running ? view_y : cyd_height(),bg,&title,1);
        if (scout_bench.running) scout_background(view_y+view_h,cyd_height(),bg,NULL,0);
        scout_round(8,6,58,28,7,button);
        scout_center(8,11,58,"Back",white,button);
        if (scout_bench.running) {
            char status[48];
            bool pal=scout_bench.frame.mode==CVBS_PAL;
            snprintf(status,sizeof(status),"%s / %s",pal?"PAL":"NTSC",pattern_names[scout_bench.pattern]);
            cyd_label(12,46,status,CYD_CYAN,bg,CYD_FONT_UI);
            cyd_label(12,68,"Wired output on P5",muted,bg,CYD_FONT_SMALL);
            scout_round(12,button_y,button_w,SCOUT_BUTTON_H,8,button);
            scout_round(right_button,button_y,button_w,SCOUT_BUTTON_H,8,button);
            scout_center(12,button_y+7,button_w,pal?"Use NTSC":"Use PAL",white,button);
            scout_center(right_button,button_y+7,button_w,"Next pattern",white,button);
        } else {
            cyd_label(12,52,"Output unavailable",CYD_YELLOW,bg,CYD_FONT_UI);
            cyd_label(12,80,"Go back and try again",muted,bg,CYD_FONT_SMALL);
        }
    }
    if (scout_bench.running) {
        uint16_t *pixels=cyd_band();
        for (int y=0;y<view_h;y+=CYD_BAND_LINES) {
            int rows=view_h-y<CYD_BAND_LINES?view_h-y:CYD_BAND_LINES;
            for (int r=0;r<rows;r++) for (int x=0;x<width;x++) {
                uint8_t g=cvbs_luma_at(&scout_bench.frame,x*CVBS_W/width,(y+r)*CVBS_H/view_h);
                pixels[r*width+x]=cyd_rgb(g,g,g);
            }
            cyd_blit(0,view_y+y,width,rows,pixels);
        }
    }
    if (buffered) cyd_frame_end(0,0);
    scout.last_draw=now;
    scout.dirty=false;
}
static void cyd_ui_init(void) {
    if (cyd_init_with_bus_hook(cyd_wallpaper_init) != ESP_OK) return;
    esp_err_t led_error=cyd_indicator_init();
    if(led_error!=ESP_OK)printf("SCOUT LED init failed: %d\n",(int)led_error);
    const char *title = "FPV Scout", *subtitle = "RF receiver";
    cyd_label((cyd_width()-cyd_label_width(title,CYD_FONT_TITLE))/2,cyd_height()/2-57,
              title,CYD_RGB(238,243,250),CYD_BLACK,CYD_FONT_TITLE);
    cyd_label((cyd_width()-cyd_label_width(subtitle,CYD_FONT_SMALL))/2,cyd_height()/2-24,
              subtitle,CYD_RGB(145,161,182),CYD_BLACK,CYD_FONT_SMALL);
    cyd_touch_calibrate(false);
    scout.bins = heap_caps_malloc(FPV_BINS * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!scout.bins) {
        cyd_fill(0, 0, cyd_width(), cyd_height(), CYD_BLACK);
        cyd_label(30, 90, "Not enough memory", CYD_RED, CYD_BLACK, CYD_FONT_TITLE);
        return;
    }
    scout_video_init();
    scout_log_load();
    scout_load_rescan();
    scout_load_start_page();
    scout_open_page(scout_start_pages[scout.start_page].page);
    scout.up = true;
}
static void cyd_ui_host_command(const char *line) {
    (void)line;
    if (!scout.up || scout.host) return;
    if (scout.page == SCOUT_REMOTE_ID || scout.page == SCOUT_MENU || scout.page == SCOUT_SIGNAL_CAL) scout_rid_stop();
    if (scout.page == SCOUT_SIGNAL_CAL &&
        (scout.cal.stage == SCOUT_CAL_CHECK || scout.cal.stage == SCOUT_CAL_BASELINE))
        scout.cal.stage = scout.cal.baseline_ready ? SCOUT_CAL_READY : SCOUT_CAL_SETUP;
    cyd_indicator_rate(0);
    scout.host = true;
    scout.video_full_state = 0;
    const scout_bg_label_t labels[] = {
        {30,80,"USB control",CYD_WHITE,CYD_FONT_TITLE},
        {30,120,"Scanning resumes when idle",CYD_GREY,CYD_FONT_UI},
    };
    scout_background(0,cyd_height(),CYD_BLACK,labels,sizeof(labels) / sizeof(labels[0]));
}
/* Read-only diagnostics do not interrupt scanning or take the host lease. */
static bool cyd_ui_command(const char *line) {
#ifdef SCOUT_VIDEO_PROBE
    if (!strcmp(line, "SCOUT ALL OPEN")) {
        scout_open_page(SCOUT_MENU);
        reply("OK\n");
        return true;
    }
    unsigned all_row;
    char trailing;
    if (!strcmp(line, "SCOUT CAL OPEN")) {
        scout_open_page(SCOUT_SETTINGS);
        scout_navigation_action(scout_navigation(SCOUT_SETTINGS,cyd_width()/2,SCOUT_SIGNAL_CAL_Y+8,cyd_landscape()));
        reply("OK\n");
        return true;
    }
    unsigned cal_tool;
    if (scout.page == SCOUT_SIGNAL_CAL && scout.cal.tool == SCOUT_CAL_MENU &&
        sscanf(line, "SCOUT CAL TOOL %u %c", &cal_tool, &trailing) == 1 &&
        cal_tool >= SCOUT_CAL_ANALOG && cal_tool <= SCOUT_CAL_RID) {
        scout_cal_input(cyd_width()/2,SCOUT_CAL_ROW_Y+(int)(cal_tool-SCOUT_CAL_ANALOG)*SCOUT_CAL_ROW_PITCH+8);
        reply("OK\n");
        return true;
    }
    if (scout.page == SCOUT_SIGNAL_CAL && (!strcmp(line, "SCOUT CAL ACTION") || !strcmp(line, "SCOUT CAL BACK"))) {
        if (!strcmp(line, "SCOUT CAL BACK")) scout_cal_input(30,20);
        else scout_cal_input(cyd_width()/2,SCOUT_BUTTON_Y+8);
        reply("OK\n");
        return true;
    }
    if (scout.page == SCOUT_MENU && sscanf(line, "SCOUT ALL ROW %u %c", &all_row, &trailing) == 1 &&
        all_row <= SCOUT_ALL_RID) {
        const bool landscape = cyd_landscape();
        const int y = SCOUT_ALL_ROW_Y + (int)all_row * SCOUT_ALL_ROW_PITCH(landscape) + 6;
        scout_navigation_action(scout_navigation(SCOUT_MENU,cyd_width()/2,y,landscape));
        reply("OK\n");
        return true;
    }
    if (!strcmp(line, "SCOUT VIDEO FULL") && scout.page == SCOUT_SCAN_PAGE && (scout.video_full || scout.video_live)) {
        scout_toggle_fullscreen();
        reply("OK\n");
        return true;
    }
    if (!strcmp(line, "SCOUT VIDEO FULL?")) {
        char status[60];
        snprintf(status,sizeof(status),"FULL active=%u state=%u rows=%u\n",scout.video_full,scout.video_full_state,sv_v.rows);
        reply(status);
        return true;
    }
    if (!strcmp(line, "SCOUT LOG OPEN")) {
        scout_open_page(SCOUT_SIGNAL_LOG);
        reply("OK\n");
        return true;
    }
    if (!strcmp(line, "SCOUT LOG SAVE") && scout.page == SCOUT_SIGNAL_LOG) {
        scout_log_input(43,SCOUT_BUTTON_Y+12);
        reply(scout.log_status);
        reply("\n");
        return true;
    }
    if (!strcmp(line, "SCOUT PERF RESET")) {
        memset(&sv_perf, 0, sizeof(sv_perf));
        sv_perf.started = esp_timer_get_time();
        reply("OK\n");
        return true;
    }
    if (!strcmp(line, "SCOUT PERF?")) {
        char report[256];
        snprintf(report, sizeof(report), "SVPERF us=%lld cap=%u cap_us=%lld decode_us=%lld lcd=%u lcd_us=%lld lcd_rows=%u gap_us=%lld phase_us=%lld rows_us=%lld decoded_rows=%u\n",
                 (long long)(esp_timer_get_time() - sv_perf.started), sv_perf.captures,
                 (long long)sv_perf.capture_us, (long long)sv_perf.decode_us, sv_perf.frames,
                 (long long)sv_perf.render_us, sv_perf.lcd_rows, (long long)sv_perf.longest_gap,
                 (long long)sv_perf.phase_us, (long long)sv_perf.rows_us, sv_perf.decoded_rows);
        reply(report);
        return true;
    }
    if (!strcmp(line, "SCOUT VIDEO AUTO")) {
        if (scout.page == SCOUT_SCAN_PAGE) {
            scout_start_auto(false);
            reply("OK\n");
        } else reply("ERR test_tune\n");
        return true;
    }
    if (!strcmp(line, "SCOUT VIDEO RECHECK") && scout.page == SCOUT_SCAN_PAGE &&
        scout.scan.mode == FPV_LOCK && !scout.manual && !scout.rechecking) {
        scout_start_recheck(esp_timer_get_time());
        reply("OK\n");
        return true;
    }
    unsigned test_mhz;
    char extra;
    if (sscanf(line, "SCOUT VIDEO TUNE %u %c", &test_mhz, &extra) == 1) {
        if (scout.page != SCOUT_SCAN_PAGE || test_mhz < SCOUT_BAND_LOW || test_mhz > SCOUT_BAND_HIGH) {
            reply("ERR test_tune\n");
        } else {
            scout.manual = true;
            scout.manual_mhz = (uint16_t)test_mhz;
            scout.skipped_mhz = 0;
            scout_clear_signal();
            reply("OK\n");
        }
        return true;
    }
    if (!strcmp(line, "SCOUT FRAME?")) {
        if (scout.page != SCOUT_SCAN_PAGE || !scout.video_live || !sv_picture) {
            reply("ERR no_video\n");
        } else {
            char header[96];
            snprintf(header,sizeof(header),"SVFRAME %d %d %u %lld %lld\n",
                     SCOUT_VIDEO_W,SCOUT_VIDEO_H,sv_v.rows,
                     (long long)sv_v.last_row_us,(long long)sv_v.tv);
            reply(header);
            sv_render_viewport(true,NULL,false);
        }
        return true;
    }
#endif
    bool reload = !strcmp(line, "WALLPAPER RELOAD");
    if (reload || !strcmp(line, "WALLPAPER?")) {
        if (reload) {
            if (!scout.up || scout.host || (scout.page != SCOUT_MENU && scout.page != SCOUT_SETTINGS)) {
                reply("ERR open_menu_or_settings\n");
                return true;
            }
            cyd_wallpaper_reload();
            scout.dirty = true;
        }
        char message[96];
        snprintf(message, sizeof(message), "WALLPAPER %s mode=%s\n", cyd_wallpaper_status(),
                 scout_wallpaper_modes[cyd_wallpaper_mode()]);
        reply(message);
        return true;
    }
    if (scout.page == SCOUT_BENCH && strcmp(line, "SCOUT?")) {
        if (strncmp(line, "BENCH ", 6)) {reply("ERR wired_bench_active\n");return true;}
        const char *command=line+6;
        bool found=false;
        for (unsigned i=0;i<PAT_COUNT;i++) if (!strcmp(command,pattern_names[i])) {
            scout_bench_select((pattern_t)i);
            found=true;
            break;
        }
        unsigned black,white;
        char extra;
        if (!found && !strcmp(command,"pal")) {scout_bench_format(CVBS_PAL);found=true;}
        if (!found && !strcmp(command,"ntsc")) {scout_bench_format(CVBS_NTSC);found=true;}
        if (!found && sscanf(command,"lvl %3u %3u %c",&black,&white,&extra)==2)
            found=scout_bench_levels(black,white);
        if (!found && !strcmp(command,"status")) found=true;
        if (found) {
            scout.dirty=true;
            char status[96];
            snprintf(status,sizeof(status),"BENCH %s %s running=%u error=%d\n",
                scout_bench.frame.mode==CVBS_PAL?"PAL":"NTSC",pattern_names[scout_bench.pattern],
                (unsigned)scout_bench.running,(int)scout_bench.error);
            reply(status);
        } else reply("ERR bench_command\n");
        return true;
    }
    if (!strncmp(line,"BENCH",5) && (line[5]==' ' || line[5]=='\0')) {
        reply("ERR open_composite_bench_on_device\n");
        return true;
    }
    if (strcmp(line, "SCOUT?")) return false;
    if (scout.page == SCOUT_SIGNAL_CAL) {
        const scout_signal_cal_t *cal = &scout.cal;
        char status[192];
        snprintf(status,sizeof(status),"FPV_SCOUT " SCOUT_BUILD_VERSION " ready=%u mode=CAL tool=%u stage=%u baseline=%u signal=%u mhz=%u quality=%u saved=%u rx_errors=%u\n",
                 scout.up,cal->tool,cal->stage,cal->baseline_ready,cal->result.signal,cal->result.mhz,
                 cal->result.quality,cal->saved_valid,scout.capture_errors);
        reply(status);
        return true;
    }
    if (scout.page == SCOUT_MENU) {
        static const char *const phases[] = {"ANALOG", "DIGITAL", "CONTROL", "REMOTEID"};
        const int64_t now = esp_timer_get_time();
        const bool analog = scout.all.analog_at && now - scout.all.analog_at < SCOUT_ALL_ANALOG_AGE_US;
        char status[230];
        snprintf(status,sizeof(status),"FPV_SCOUT " SCOUT_BUILD_VERSION " ready=%u mode=ALL phase=%s analog_mhz=%u digital_mhz=%u elrs_hops=%u ctrl_passes=%u remote_reports=%u passes=%u errors=%u heap=%u\n",
                 scout.up,phases[scout.all.phase],analog ? scout_channels[scout.all.analog_channel].mhz : 0,
                 scout.all.digital.shown.mhz,scout.control.elrs_hops,scout.control.passes,scout_rid_recent(now),
                 scout.sweeps,scout.capture_errors,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        reply(status);
        return true;
    }
    if (scout.page == SCOUT_SIGNAL_LOG) {
        char status[92];
        snprintf(status,sizeof(status),"FPV_SCOUT " SCOUT_BUILD_VERSION " ready=%u mode=LOG entries=%u run=%lu first_run=%lu\n",
                 scout.up,(unsigned)scout_log.count,(unsigned long)scout_log.run,
                 (unsigned long)(scout_log.count ? scout_log.entries[0].first_run : 0));
        reply(status);
        return true;
    }
    if (scout.page == SCOUT_REMOTE_ID) {
        char status[230];
        snprintf(status,sizeof(status),"FPV_SCOUT " SCOUT_BUILD_VERSION " ready=%u mode=REMOTEID active=%u failed=%u channel=%u frames=%u matches=%u ble=%u ble_active=%u ble_frames=%u ble_matches=%u\n",
                 scout.up,scout_rid != NULL,scout_rid == NULL,scout_rid ? scout_rid->channel : 0,
                 scout_rid_frames(),scout_rid ? scout_rid->matches : 0,
                 scout_rid && scout_rid->ble_enabled,scout_rid && scout_rid->ble_window,
                 scout_rid_ble_frames(),scout_rid ? scout_rid->ble_matches : 0);
        reply(status);
        return true;
    }
    if (scout.page == SCOUT_CONTROL) {
        const scout_control_t *c = &scout.control;
        char status[340];
        snprintf(status,sizeof(status),"FPV_SCOUT " SCOUT_BUILD_VERSION " ready=%u mode=CONTROL ctrl_passes=%u ctrl_errors=%u waterfall_rows=%u elrs_hops=%u elrs_sf=%u elrs_prob=%u elrs_dbfs=%d elrs_lq=%u dji_matches=%u dji_prob=%u dji_dbfs=%d dji_lq=%u\n",
                 scout.up,c->passes,c->errors,scout.waterfall_count,c->elrs_hops,c->elrs_sf,
                 c->elrs_hops ? scout_control_probability(&c->elrs,c->passes) : 0,c->elrs.dbfs,c->elrs.quality,
                 c->dji_matches,c->dji_matches ? scout_control_probability(&c->dji,c->passes) : 0,c->dji.dbfs,c->dji.quality);
        reply(status);
        return true;
    }
    static const char *const modes[] = {"SCAN", "VERIFY", "HIT", "LOCK"};
    char status[384], rssi[12] = "--", lq[8] = "--";
    const scout_channel_t channel = scout_current_channel();
    const int64_t now = esp_timer_get_time();
    if (scout.scan.mode == FPV_LOCK) scout_signal_labels(now, rssi, lq);
    const char *mode = scout.tuner_open ? "TUNE" : scout.page == SCOUT_SPECTRUM ? (scout.spectrum_held ? "HOLD" : "SPECTRUM") :
        scout.rechecking ? "RECHECK" : scout.comparing ? "COMPARE" : modes[scout.scan.mode];
    static const char *const rf_kinds[] = {"NONE", "UNKNOWN", "O4_LIKE", "AVATAR_LIKE"};
    const scout_rf_hit_t rf = scout.digital.shown;
    snprintf(status, sizeof(status), "FPV_SCOUT " SCOUT_BUILD_VERSION " ready=%u mode=%s freq=%u passes=%u pass_ms=%u captures=%u errors=%u heap=%u control=%s channel=%s rows=%u hsync=%u vsync=%u rescan=%s contenders=%u compare_ms=%u lock_ms=%u rssi=%s lq=%s rf=%s rf_mhz=%u rf_width_khz=%u sync_age_ms=%lld video_age_ms=%lld\n",
             scout.up, mode, channel.mhz, scout.sweeps,
             (unsigned)(scout.sweep_us / 1000), scout.captures, scout.capture_errors,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             scout.manual ? "MANUAL" : "AUTO",
             channel.name, sv_v.rows,
             (unsigned)(sv_v.lv_ok && now - sv_v.last_sync_us < 500000),
             (unsigned)sv_v.tv_ok, scout.auto_rescan ? "ON" : "OFF",
             scout.last_compared, (unsigned)(scout.compare_us / 1000), (unsigned)(scout.lock_us / 1000), rssi, lq,
             rf_kinds[rf.kind], rf.mhz, rf.width_khz,
             (long long)(scout.scan.last_sync ? (now - scout.scan.last_sync) / 1000 : -1),
             (long long)(sv_v.last_row_us ? (now - sv_v.last_row_us) / 1000 : -1));
    reply(status);
    return true;
}

static bool scout_read_evidence(fpv_evidence_t *evidence) {
    uint32_t elapsed;
    *evidence = (fpv_evidence_t){0};
    if (fast_capture(FPV_SAMPLES, 2, &elapsed)) {
        scout.errors++;
        scout.capture_errors++;
        return false;
    }
    scout.captures++;
    scout.errors = 0;
    *evidence = fpv_detect(DUMP_BUF, scout.bins);
    return true;
}

static void scout_detect_channel(void) {
    fpv_evidence_t evidence;
    unsigned before = scout.scan.mode;
    const bool captured = scout_read_evidence(&evidence);
    if (before == FPV_LOCK) {
        const bool good = captured && evidence.polarity == scout.scan.polarity &&
                          (evidence.h_pairs >= 4 || evidence.v_pairs >= 2);
        scout.lq_history = (uint16_t)((scout.lq_history << 1) | good);
        if (scout.lq_samples < 16) scout.lq_samples++;
        if (good) scout_log_analog(esp_timer_get_time());
    }
    if (captured) {
        if (fpv_observe(&scout.scan, evidence, esp_timer_get_time())) scout_confirmed();
    } else if (scout.scan.mode == FPV_VERIFY) {
        fpv_resume(&scout.scan);
    }
    if (!scout.manual && before == FPV_VERIFY && scout.scan.mode == FPV_SCAN) scout_start_auto(false);
    if (before != scout.scan.mode) scout.dirty = true;
}

static void scout_scan_refresh(int64_t now) {
    if (scout.page == SCOUT_MENU || scout.page == SCOUT_SIGNAL_CAL) return;
    const bool held = scout.scan.mode == FPV_HIT || scout.scan.mode == FPV_LOCK;
    /* Leave the tuner on the video channel while acquiring or recovering field sync. */
    const bool fresh_video = sv_v.tv_ok && sv_v.last_row_us && scout.scan.last_sync &&
                             now - sv_v.last_row_us < SCOUT_VIDEO_RECENT_US &&
                             now - scout.scan.last_sync < SCOUT_VIDEO_RECENT_US;
    const int64_t sweep_period = scout.scan.mode == FPV_SCAN ? 20000 : 200000;
    if (scout.scan.mode != FPV_VERIFY && (!held || fresh_video) &&
        now - scout.spectrum_tick >= sweep_period)
        scout_update_spectrum(now);
    if (scout.dirty || now - scout.last_draw >= 150000)
        scout_draw(esp_timer_get_time());
    if (scout.video_live) {
        if (scout.video_full) {
            const scout_channel_t channel = scout_current_channel();
            char strength[12], quality[8], overlay[64];
            scout_signal_labels(now,strength,quality);
            snprintf(overlay,sizeof(overlay),"%u MHz   RSSI %s   LQ %s",channel.mhz,strength,quality);
            scout_video_render_mode(esp_timer_get_time(),overlay);
        } else scout_video_render(esp_timer_get_time());
    }
    bool sync=scout.scan.last_sync && now-scout.scan.last_sync<1000000;
    const int32_t strength = scout_rssi(now);
    cyd_indicator_rate(scout_indicator_rate(held && sync && !scout.errors && strength != INT32_MIN,
        strength, now - scout.spectrum_updated));
}

static void scout_shortlist(fpv_evidence_t evidence) {
    if (evidence.h_pairs < 4 && evidence.v_pairs < 2) return;
    const int mhz = (int)scout_channels[scout.channel].mhz;
    if (abs(mhz - (int)scout.skipped_mhz) <= 10) return;
    if (scout.rechecking && mhz == (int)scout_channels[scout.recheck_channel].mhz) return;
    for (unsigned i = 0; i < scout.candidate_count; i++)
        if (mhz == (int)scout_channels[scout.candidates[i].channel].mhz) return;

    const int32_t strength = scout_measure_signal();
    if (strength == INT32_MIN) return;
    unsigned index = 0;
    while (index < scout.candidate_count && strength <= scout.candidates[index].strength)
        index++;
    if (index == SCOUT_CONTENDERS) return;
    if (scout.candidate_count < SCOUT_CONTENDERS) scout.candidate_count++;
    for (unsigned i = scout.candidate_count - 1; i > index; i--)
        scout.candidates[i] = scout.candidates[i - 1];
    scout.candidates[index] = (scout_candidate_t){
        .channel = scout.channel, .strength = strength,
        .sync = {.mode=FPV_VERIFY, .polarity=evidence.polarity, .deadline=INT64_MAX},
    };
}

static void scout_complete_recheck(int winner, int64_t now) {
    if (winner >= 0) {
        const scout_candidate_t chosen = scout.candidates[winner];
        printf("SCOUT recheck switched %u -> %u MHz\n",
               scout_channels[scout.recheck_channel].mhz, scout_channels[chosen.channel].mhz);
        scout_clear_signal();
        scout.channel = chosen.channel;
        scout.scan = chosen.sync;
        scout_confirmed();
    } else {
        scout.channel = scout.recheck_channel;
        scout.rechecking = scout.comparing = false;
        scout.candidate_count = 0;
        scout.tuned = false;
        scout.next_check = 0;
        scout.dirty = true;
        printf("SCOUT recheck stayed %u MHz\n", scout_channels[scout.channel].mhz);
    }
    scout.recheck_after = now + SCOUT_RECHECK_COOLDOWN_US;
}

static unsigned scout_check_coverage(const scout_candidate_t *candidate) {
    return 100 * candidate->valid_h / candidate->observations;
}

static unsigned scout_check_picture(const scout_candidate_t *candidate) {
    return candidate->video_checks ? 256 * candidate->clean_lines / candidate->video_checks : 0;
}

static int scout_recheck_winner(void) {
    const unsigned count = scout.candidate_count - 1;
    const scout_candidate_t *current = &scout.candidates[count];
    if (current->observations < SCOUT_RECHECK_MIN_CHECKS) return -1;
    const unsigned current_coverage = scout_check_coverage(current);
    const unsigned current_picture = scout_check_picture(current);
    int best = -1;
    unsigned best_coverage = 0, best_picture = 0;
    for (unsigned i = 0; i < count; ++i) {
        const scout_candidate_t *candidate = &scout.candidates[i];
        if (candidate->sync.mode != FPV_HIT || candidate->observations < SCOUT_RECHECK_MIN_CHECKS)
            continue;
        const unsigned coverage = scout_check_coverage(candidate);
        const unsigned picture = scout_check_picture(candidate);
        const bool comparable_picture = current->video_checks >= SCOUT_COMPARE_VIDEO_MIN &&
                                        candidate->video_checks >= SCOUT_COMPARE_VIDEO_MIN;
        const bool steadier = coverage >= current_coverage + SCOUT_RECHECK_GAIN &&
                              (!comparable_picture || picture + 128 >= current_picture);
        const bool cleaner = comparable_picture && coverage + 5 >= current_coverage &&
                             picture >= current_picture + 256;
        if ((steadier || cleaner) && (best < 0 || coverage > best_coverage ||
            (coverage == best_coverage && picture > best_picture))) {
            best = (int)i;
            best_coverage = coverage;
            best_picture = picture;
        }
    }
    /* The shortlist's RSSI order resolves equivalent improvements. */
    return best;
}

static void scout_finish_comparison(int64_t now) {
    if (scout.rechecking) {
        scout.last_compared = scout.candidate_count;
        scout.compare_us = now - scout.compare_start;
        const int winner = scout_recheck_winner();
        const int64_t last_sync = scout.candidates[scout.candidate_count - 1].sync.last_sync;
        if (winner < 0 && last_sync > scout.scan.last_sync) scout.scan.last_sync = last_sync;
        scout_complete_recheck(winner, now);
        return;
    }
    int best = -1;
    unsigned best_quality = 0;
    /* Candidates are already ordered by RF strength. Use it to break ties. */
    for (unsigned i = 0; i < scout.candidate_count; i++) {
        const scout_candidate_t *c = &scout.candidates[i];
        if (c->sync.mode != FPV_HIT || !c->observations) continue;
        const unsigned quality = 256 * c->h_pairs / c->observations;
        if (best < 0 || quality > best_quality) {
            best = (int)i;
            best_quality = quality;
        }
    }
    if (best >= 0 && scout.candidates[best].video_checks >= SCOUT_COMPARE_VIDEO_MIN) {
        const int reference = best;
        const int mhz = scout_channels[scout.candidates[reference].channel].mhz;
        unsigned clean = 256 * scout.candidates[reference].clean_lines / scout.candidates[reference].video_checks;
        const unsigned minimum = clean + 256;
        /* Overlapping frequencies can decode the same sync while clipping the picture. */
        for (unsigned i = 0; i < scout.candidate_count; ++i) {
            const scout_candidate_t *c = &scout.candidates[i];
            if ((int)i == reference || c->sync.mode != FPV_HIT || !c->observations ||
                c->video_checks < SCOUT_COMPARE_VIDEO_MIN ||
                abs((int)scout_channels[c->channel].mhz - mhz) > SCOUT_NEARBY_MHZ) continue;
            const unsigned score = 256 * c->clean_lines / c->video_checks;
            if (score >= minimum && score > clean) {
                best = (int)i;
                clean = score;
                best_quality = 256 * c->h_pairs / c->observations;
            }
        }
    }
    scout.last_compared = scout.candidate_count;
    scout.compare_us = now - scout.compare_start;
    scout.lock_us = best >= 0 ? now - scout.sweep_start : 0;
    if (best < 0) {
        scout_start_auto(false);
    } else {
        const scout_candidate_t winner = scout.candidates[best];
        scout_clear_signal();
        scout.channel = winner.channel;
        scout.scan = winner.sync;
        scout_confirmed();
        printf("SCOUT cleanest %s %u contenders=%u quality=%u\n",
               scout_channels[scout.channel].name, scout_channels[scout.channel].mhz,
               scout.last_compared, best_quality);
    }
    scout_scan_refresh(now);
}

static void scout_compare_step(int64_t now) {
    if (scout.tuned && now >= scout.compare_until) {
        scout.tuned = false;
        if (++scout.compare_index == scout.candidate_count) {
            scout.compare_index = 0;
            if (++scout.compare_round == SCOUT_COMPARE_ROUNDS) {
                scout_finish_comparison(now);
                return;
            }
        }
    }
    scout_candidate_t *candidate = &scout.candidates[scout.compare_index];
    if (!scout.tuned) {
        scout.channel = candidate->channel;
        scout_tune();
        scout.compare_until = esp_timer_get_time() + SCOUT_COMPARE_DWELL_US;
        scout.compare_next_video = scout.compare_until - SCOUT_COMPARE_DWELL_US + 3000;
    }
    fpv_evidence_t evidence;
    const bool captured = scout_read_evidence(&evidence);
    if (scout.rechecking && !captured) return;
    fpv_observe(&candidate->sync, evidence, esp_timer_get_time());
    /* Expected vertical blanking windows carry little H evidence. */
    if (evidence.polarity != candidate->sync.polarity || evidence.v_pairs < 2 || evidence.h_pairs >= 4) {
        candidate->observations++;
        if (evidence.polarity == candidate->sync.polarity) {
            candidate->h_pairs += evidence.h_pairs;
            if (evidence.h_pairs >= 4) candidate->valid_h++;
        }
    }
    if (captured && now >= scout.compare_next_video && (evidence.v_pairs < 2 || evidence.h_pairs >= 4)) {
        scout.compare_next_video = now + SCOUT_COMPARE_VIDEO_US;
        candidate->video_checks++;
        if (evidence.polarity == candidate->sync.polarity && evidence.h_pairs >= 4)
            candidate->clean_lines += scout_video_quality(evidence.polarity);
    }
}

static void scout_sweep_step(int64_t now) {
    if (!scout.channel && !scout.rechecking) {
        if (scout.dirty) scout_scan_refresh(now);
        scout.sweep_start = now = esp_timer_get_time();
    }
    scout_tune();
    fpv_evidence_t evidence;
    if (scout_read_evidence(&evidence)) scout_shortlist(evidence);
    if (scout.page == SCOUT_SCAN_PAGE && !scout.rechecking && now - scout.sweep_label_at >= 75000) {
        scout_draw_scan_channel(true);
        scout.sweep_label_at = now;
        if (now - scout.spectrum_tick >= 75000) scout_update_spectrum(now);
    }
    scout.tuned = false;
    if (++scout.channel < SCOUT_COUNT) return;

    now = esp_timer_get_time();
    scout.sweeps++;
    scout.sweep_us = now - scout.sweep_start;
    scout.channel = 0;
    scout.skipped_mhz = 0;
    if (scout.rechecking) {
        if (!scout.candidate_count) {
            scout_complete_recheck(-1, now);
            return;
        }
        scout.candidates[scout.candidate_count++] = (scout_candidate_t){
            .channel = scout.recheck_channel,
            .sync = {.mode=FPV_VERIFY, .polarity=scout.scan.polarity, .deadline=INT64_MAX},
        };
    }
    if (scout.candidate_count) {
        scout.comparing = true;
        scout.compare_index = scout.compare_round = 0;
        scout.channel = scout.candidates[0].channel;
        scout.dirty = true;
        if (scout.page == SCOUT_SCAN_PAGE) scout_draw(now);
        scout.compare_start = esp_timer_get_time();
    } else {
        scout.last_compared = 0;
        scout.compare_us = scout.lock_us = 0;
        scout_scan_refresh(now);
    }
}

static void scout_spectrum_page_step(int64_t now) {
    const bool updated = !scout.spectrum_held && scout_update_spectrum(now);
    if (scout.dirty || updated) scout_draw_spectrum_page(now);
}

#ifdef SCOUT_RUST_CORE
static void scout_control_measure(scout_control_reading_t *signal, scout_rf_quality_t quality) {
    const uint64_t full_scale = 2u * 512u * 512u;
    const uint32_t power = quality.power ? quality.power : 1;
    signal->dbfs = (db100_u64(power) - db100_u64(full_scale) - 50) / 100;
    signal->quality = quality.coherence;
    signal->seen = true;
}

static void scout_control_capture(int64_t now) {
    scout_control_t *c = &scout.control;
    const unsigned block = c->step / 21, channel = c->step % 21;
    const bool dji = channel == 20;
    const unsigned elrs_channel = ((block * 20 + channel) * 19 + c->phase) % 80;
    frequency_mhz = dji ? 2410 + 21 * block : 2400 + elrs_channel;
    c5_khz_offset = 0;
    rx_channel_mode = rx_bandwidth_phy_mode(dji ? 40 : 20);
    rx_analog_filter = rx_bandwidth_dcap(dji ? 40 : 20);
    rx_ready = false;
    prepare_rx();
    uint32_t elapsed;
    scout_rf_quality_t quality;
    if (dji) {
        if (spec_compute(1,256,4,spec_bins,&elapsed)) {
            c->errors++;
        } else {
            scout_rf_shape_t shape;
            scout_rf_shape(spec_bins,&shape);
            if (shape.width_khz >= 6250) {
                if (fast_capture(FPV_SAMPLES,1,&elapsed)) {
                    c->errors++;
                } else if (scout_rf_timing(DUMP_BUF,FPV_SAMPLES,SCOUT_TIMING_O4 | SCOUT_TIMING_DJI_15,&quality)) {
                    const unsigned mhz = ((int)frequency_mhz * 1000 + shape.offset_khz + 500) / 1000;
                    if (mhz >= 2400 && mhz <= 2484) {
                        if (!c->dji_matches || !scout_rf_near(mhz,c->dji_mhz)) {
                            c->dji_mhz = mhz;
                            c->dji_matches = 1;
                        } else c->dji_matches = 2;
                        c->dji_at = now;
                        scout_control_measure(&c->dji,quality);
                    }
                }
            }
        }
    } else {
        unsigned got;
        // The ELRS channel plan starts at 2400.4 MHz. The DDC recenters
        // and filters a single hop before the LoRa timing check.
        if (!ddc_snapshot(16,1600,2,10,400000,&elapsed,&got)) {
            c->errors++;
        } else {
            unsigned sf = scout_elrs_chirps(DUMP_BUF,got,sin_tab,&quality);
            if (sf) {
                if (c->elrs_sf != sf) c->elrs_hops = 0;
                if (!c->elrs_hops) {
                    c->elrs_mhz = frequency_mhz;
                    c->elrs_hops = 1;
                } else if (abs((int)c->elrs_mhz - (int)frequency_mhz) >= 2) {
                    c->elrs_hops = 2;
                }
                c->elrs_sf = sf;
                c->elrs_at = now;
                scout_control_measure(&c->elrs,quality);
            }
        }
    }
    if (++c->step == 84) {
        c->step = 0;
        c->phase = (c->phase + 7) % 80;
        c->elrs.history = (c->elrs.history << 1) | c->elrs.seen;
        c->dji.history = (c->dji.history << 1) | c->dji.seen;
        c->elrs.seen = c->dji.seen = false;
        c->passes++;
    }
}
#endif

static void scout_control_step(int64_t now) {
    scout_control_t *c = &scout.control;
    const unsigned had_elrs = c->elrs_hops, had_dji = c->dji_matches;
    if (now - c->elrs_at >= 8000000) c->elrs_hops = 0;
    if (now - c->dji_at >= 8000000) c->dji_matches = 0;
    if (had_elrs >= 2 && !c->elrs_hops) scout_log_lost(SCOUT_LOG_ELRS,0,now);
    if (had_dji >= 2 && !c->dji_matches) scout_log_lost(SCOUT_LOG_DJI_CONTROL,c->dji_mhz,now);
    bool updated = false;
    if (scout.page == SCOUT_CONTROL && now - scout.spectrum_tick >= 150000) {
        updated = scout_update_spectrum(now);
    }
#ifdef SCOUT_RUST_CORE
    else scout_control_capture(now);
#endif
    if (c->elrs_hops >= 2 && c->elrs_at == now) scout_log_record(SCOUT_LOG_ELRS, 0, c->elrs.dbfs, now);
    if (c->dji_matches >= 2 && c->dji_at == now) scout_log_record(SCOUT_LOG_DJI_CONTROL, c->dji_mhz, c->dji.dbfs, now);
    if (scout.page == SCOUT_CONTROL && (scout.dirty || updated || had_elrs != c->elrs_hops || had_dji != c->dji_matches))
        scout_draw_control(updated);
}

static void scout_remote_id_step(int64_t now) {
    const unsigned matches = scout_rid ? scout_rid->matches : 0;
    const unsigned ble = scout_rid ? scout_rid->ble_matches : 0;
    scout_rid_poll(now);
    if (scout_rid && scout_rid->ble_matches != ble) {
        scout_log_record(SCOUT_LOG_RID_BLE,0,scout_rid->latest_ble_rssi,now);
        scout.rid_log_seen[1] = now;
    }
    if (scout_rid && scout_rid->matches - scout_rid->ble_matches != matches - ble) {
        const unsigned ch = scout_rid->latest_wifi_channel;
        const unsigned mhz = !ch ? 0 : ch < 14 ? 2407 + ch * 5 : ch == 14 ? 2484 : 5000 + ch * 5;
        scout_log_record(SCOUT_LOG_RID_WIFI,mhz,scout_rid->latest_wifi_rssi,now);
        scout.rid_log_seen[0] = now;
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (scout.rid_log_seen[i] && now - scout.rid_log_seen[i] >= SCOUT_RID_EXPIRY_US) {
            scout_log_lost(SCOUT_LOG_RID_WIFI+i,0,now);
            scout.rid_log_seen[i] = 0;
        }
    }
}

static bool scout_cal_check_reference(int64_t now) {
    scout_signal_cal_t *cal = &scout.cal;
    scout_cal_reference_t *r = &cal->result;
    switch (cal->tool) {
    case SCOUT_CAL_ANALOG:
        if (scout.comparing) {
            scout_compare_step(now);
        } else if (scout.scan.mode != FPV_LOCK) {
            scout_sweep_step(now);
        } else {
            if (!scout.tuned) scout_tune();
            if (now >= cal->next_sample) {
                scout_detect_channel();
                cal->next_sample = now + 100000;
            }
            scout_video_capture();
            const int lq = scout_lq();
            if (scout.lq_samples == 16 && lq >= 75 && sv_v.tv_ok &&
                sv_v.rows * 5 >= (unsigned)SCOUT_VIDEO_H * 4) {
                r->mhz = scout_current_channel().mhz;
                r->signal = sv_v.pal ? 1 : 2;
                r->quality = lq;
                r->level = 100 * sv_v.rows / SCOUT_VIDEO_H;
                return true;
            }
        }
        break;
    case SCOUT_CAL_DIGITAL: {
        scout_spectrum_step(now);
        const scout_rf_hit_t hit = scout.digital.shown;
        if (hit.kind && hit.mhz) {
            r->signal = hit.kind;
            r->mhz = hit.mhz;
            r->level = hit.contrast > INT16_MAX ? INT16_MAX : hit.contrast;
            return true;
        }
        break;
    }
    case SCOUT_CAL_CONTROL: {
#ifdef SCOUT_RUST_CORE
        scout_control_capture(now);
#endif
        const scout_control_t *c = &scout.control;
        if (c->elrs_hops >= 2 || c->dji_matches >= 2) {
            const bool elrs = c->elrs_hops >= 2;
            const scout_control_reading_t *reading = elrs ? &c->elrs : &c->dji;
            r->signal = elrs ? 1 : 2;
            r->level = reading->dbfs;
            r->quality = reading->quality;
            return true;
        }
        break;
    }
    case SCOUT_CAL_RID:
        scout_rid_poll(now);
        if (scout_rid) {
            if (scout_rid->matches > scout_rid->ble_matches) {
                r->signal |= 1;
                r->level = scout_rid->latest_wifi_rssi;
            }
            if (scout_rid->ble_matches) {
                r->signal |= 2;
                r->other_level = scout_rid->latest_ble_rssi;
            }
        }
        return r->signal == 3;
    }
    return false;
}

static void scout_cal_step(int64_t now) {
    scout_signal_cal_t *cal = &scout.cal;
    const bool active = cal->stage == SCOUT_CAL_BASELINE || cal->stage == SCOUT_CAL_CHECK;
    if (active && now >= cal->deadline) {
        scout_cal_finish(cal->tool == SCOUT_CAL_RID && cal->result.signal);
    } else if (cal->stage == SCOUT_CAL_BASELINE) {
        if (!scout.spectrum_step) cal->baseline_error = false;
        scout_spectrum_step(now);
        if (!scout.spectrum_step && scout.spectrum_valid && !cal->baseline_error) {
            if (cal->quiet_plot)
                for (unsigned i = 0; i < SCOUT_SPEC_W; ++i)
                    if (scout.spectrum_plot[i] > cal->quiet_plot[i]) cal->quiet_plot[i] = scout.spectrum_plot[i];
            cal->baseline_total += scout.floor_est;
            if (++cal->baseline_passes == 3) {
                cal->baseline_ready = true;
                cal->result.floor = cal->baseline_total / cal->baseline_passes;
                cal->stage = SCOUT_CAL_READY;
                scout.dirty = true;
            }
        }
    } else if (cal->stage == SCOUT_CAL_CHECK && scout_cal_check_reference(now)) {
        scout_cal_finish(true);
    }
    if (scout.dirty || (active && now - scout.last_draw >= 300000)) scout_draw_calibration(now);
}

/* Take turns on the same PHY. The normal scanner pages run their own routines. */
static void scout_all_step(int64_t now) {
    scout_all_t *all = &scout.all;
    const unsigned previous_phase = all->phase;
    switch (all->phase) {
    case SCOUT_ALL_ANALOG: {
        if (scout.comparing) {
            scout_compare_step(now);
            if (scout.comparing) break;
        } else {
            const unsigned sweeps = scout.sweeps;
            scout_sweep_step(now);
            if (scout.sweeps == sweeps || scout.comparing) break;
        }
        if (scout.scan.mode == FPV_LOCK) {
            all->analog_channel = scout.channel;
            all->analog_polarity = scout.scan.polarity;
            all->analog_at = now;
        }
        scout.spectrum_step = 0;
        all->phase = SCOUT_ALL_DIGITAL;
        break;
    }
    case SCOUT_ALL_DIGITAL:
        scout_spectrum_step(now);
        if (!scout.spectrum_step) all->phase = SCOUT_ALL_CONTROL;
        break;
    case SCOUT_ALL_CONTROL:
        scout_control_step(now);
#ifdef SCOUT_RUST_CORE
        if (scout.control.step) break;
#endif
        scout_rid_start();
        all->rid_until = esp_timer_get_time() + SCOUT_ALL_RID_US;
        all->phase = SCOUT_ALL_RID;
        break;
    case SCOUT_ALL_RID:
        if (now >= all->rid_until) {
            scout_rid_suspend();
            scout_start_auto(false);
            all->phase = SCOUT_ALL_ANALOG;
        } else {
            const unsigned reports = scout_rid_recent(scout.last_draw);
            scout_remote_id_step(now);
            if (scout_rid_recent(now) != reports) scout.dirty = true;
        }
        break;
    }
    if (scout.dirty || previous_phase != all->phase) scout_draw_all(now);
}

static void cyd_ui_step(void) {
    if (!scout.up) return;
    int64_t now = esp_timer_get_time();
    if (scout.host) {
        scout.host = false;
        if (scout.rechecking) scout_complete_recheck(-1, now);
        scout.tuned = false;
        scout.next_check = 0;
        scout.dirty = true;
        /* Never carry partial evidence over a host capture or retune. */
        if (scout.scan.mode == FPV_VERIFY) fpv_resume(&scout.scan);
        scout.scan.last_sync = 0;
        scout.lq_history = scout.lq_samples = 0;
        scout.hold_started = now;
        scout_video_reset();
        scout.video_live = false;
        cyd_reclaim_pins();
        if (scout.page == SCOUT_REMOTE_ID) {
            scout.rid_log_seen[0] = scout.rid_log_seen[1] = 0;
            scout_rid_start();
        }
        if (scout.page == SCOUT_MENU) scout_open_page(SCOUT_MENU);
        if (scout.page == SCOUT_SCAN_PAGE && !scout.manual && scout.scan.mode == FPV_SCAN)
            scout_start_auto(false);
    }
    scout_input();
    scout_log_checkpoint(now);
    if (scout.tuner_open) {
        if (scout_wallpaper_visible() && cyd_frame_ready() && cyd_wallpaper_update(now)) scout.dirty = true;
        if (scout.dirty) scout_draw_tuner();
        return;
    }
    if (scout.page != SCOUT_SCAN_PAGE && scout_wallpaper_visible() &&
        (scout.page == SCOUT_MENU || cyd_frame_ready()) && cyd_wallpaper_update(now))
        scout.dirty = true;
    if (scout.page == SCOUT_BENCH) {
        scout_bench_step(now);
        if (scout.dirty || (scout_bench.pattern == PAT_BOUNCE && now-scout.last_draw>=200000))
            scout_draw_bench(now);
        return;
    }
    if (scout.page == SCOUT_SPECTRUM) {
        scout_spectrum_page_step(now);
        return;
    }
    if (scout.page == SCOUT_CONTROL) {
        scout_control_step(now);
        return;
    }
    if (scout.page == SCOUT_MENU) {
        scout_all_step(now);
        return;
    }
    if (scout.page == SCOUT_REMOTE_ID) {
        scout_remote_id_step(now);
        if (scout.dirty || now - scout.last_draw >= 500000) scout_draw_remote_id(now);
        return;
    }
    if (scout.page == SCOUT_SIGNAL_CAL) {
        scout_cal_step(now);
        return;
    }
    if (scout.page == SCOUT_SIGNAL_LOG) {
        if (scout.dirty || now - scout.last_draw >= 1000000) scout_draw_signal_log(now);
        return;
    }
    if (scout.page!=SCOUT_SCAN_PAGE) {
        cyd_indicator_rate(0);
        if (scout.dirty) scout_draw_settings();
        return;
    }
    if (scout.scan_plot_dirty && !scout.dirty) scout_draw_scan_plot();
    if (scout.rechecking) {
        if (now - scout.sweep_start >= SCOUT_RECHECK_MAX_US) scout_complete_recheck(-1, now);
        else if (scout.comparing) scout_compare_step(now);
        else scout_sweep_step(now);
        if (scout.dirty) scout_draw(now);
        return;
    }
    if (scout.comparing) {
        scout_compare_step(now);
        if (scout.dirty) scout_draw(now);
        return;
    }
    if (!scout.manual && scout.scan.mode == FPV_SCAN) {
        scout_sweep_step(now);
        return;
    }
    if (scout.scan.mode == FPV_VERIFY && now >= scout.scan.deadline) {
        if (!scout.manual) {
            scout_start_auto(false);
            return;
        }
        fpv_resume(&scout.scan);
        scout.dirty = true;
    }
    const int64_t check_started = esp_timer_get_time();
    if (!scout.tuned) scout_tune();
    bool held = scout.scan.mode == FPV_HIT || scout.scan.mode == FPV_LOCK;
    if (!held || check_started >= scout.next_check) {
        if (held) scout.next_check = check_started + 100000;
        scout_detect_channel();
    }
    if (scout.scan.mode == FPV_HIT || scout.scan.mode == FPV_LOCK) {
        const int64_t previous_row = sv_v.last_row_us;
        if (!scout_video_capture()) scout.capture_errors++;
        /* New decoded rows with a current horizontal pulse train also confirm sync. */
        if (sv_v.last_row_us > previous_row && sv_v.last_sync_us == sv_v.last_row_us &&
            sv_v.last_row_us > scout.scan.last_sync)
            scout.scan.last_sync = sv_v.last_row_us;
        now = esp_timer_get_time();
        const int64_t last_sync = scout.scan.last_sync ? scout.scan.last_sync : scout.hold_started;
        if (now - last_sync >= sv_FRAME_HOLD_US)
            scout_log_lost(SCOUT_LOG_ANALOG,scout_current_channel().mhz,now);
        if (scout.auto_rescan && now - last_sync >= SCOUT_SYNC_TIMEOUT_US) {
            scout_start_auto(false);
            scout_scan_refresh(now);
            return;
        }
        bool live = scout_video_live(now);
        if (live != scout.video_live) scout.dirty = true;
        scout.video_live = live;
        scout.video_pal = sv_v.pal;
        if (scout_recheck_due(now)) {
            scout_start_recheck(now);
            scout_draw(now);
            return;
        }
    }
    scout_scan_refresh(esp_timer_get_time());
}
