/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { SCOUT_LOG_CAPACITY = 32 };
#define SCOUT_LOG_NO_LEVEL INT32_MIN
enum {
    SCOUT_LOG_ANALOG, SCOUT_LOG_DIGITAL, SCOUT_LOG_O4, SCOUT_LOG_AVATAR,
    SCOUT_LOG_ELRS, SCOUT_LOG_DJI_CONTROL, SCOUT_LOG_RID_WIFI, SCOUT_LOG_RID_BLE
};

typedef struct {
    uint32_t first_run, first_seconds;
    uint32_t seen_run, seen_seconds;
    uint32_t lost_run, lost_seconds;
    int32_t strength;
    uint16_t mhz;
    uint8_t kind;
} scout_log_entry_t;

typedef struct {
    uint32_t run, count;
    scout_log_entry_t entries[SCOUT_LOG_CAPACITY];
} scout_log_t;

static scout_log_t scout_log = { .run = 1 };
static bool scout_log_dirty, scout_log_urgent;

static const char *const scout_log_names[] = {
    "Analog video", "Digital signal", "Likely DJI O4", "Likely Avatar",
    "Likely ExpressLRS", "Likely DJI control", "Remote ID / Wi-Fi", "Remote ID / Bluetooth"
};

static bool scout_log_video_digital(unsigned kind) {
    return kind >= SCOUT_LOG_DIGITAL && kind <= SCOUT_LOG_AVATAR;
}

static unsigned scout_log_find(unsigned kind, unsigned mhz) {
    for (unsigned index = 0; index < scout_log.count; index++) {
        const scout_log_entry_t *entry = &scout_log.entries[index];
        if (entry->kind == kind && entry->mhz == mhz) return index;
        if (kind >= SCOUT_LOG_RID_WIFI && entry->kind == kind) return index;
        if (scout_log_video_digital(kind) && scout_log_video_digital(entry->kind) &&
            abs((int)entry->mhz - (int)mhz) <= 5) return index;
    }
    return scout_log.count;
}

static void scout_log_record(unsigned kind, unsigned mhz, int32_t strength, int64_t now) {
    const unsigned index = scout_log_find(kind, mhz);
    const uint32_t seconds = now / 1000000;
    scout_log_entry_t entry;
    if (index == scout_log.count) {
        if (index == SCOUT_LOG_CAPACITY) return;
        scout_log.count++;
        entry = (scout_log_entry_t){.first_run=scout_log.run,.first_seconds=seconds,.strength=SCOUT_LOG_NO_LEVEL};
        scout_log_urgent = true;
    } else {
        entry = scout_log.entries[index];
        if (!index && entry.kind == kind && entry.mhz == mhz && entry.seen_run == scout_log.run &&
            entry.seen_seconds == seconds && (strength == SCOUT_LOG_NO_LEVEL || entry.strength == strength)) return;
    }
    entry.kind = kind;
    entry.mhz = mhz;
    entry.seen_seconds = seconds;
    entry.seen_run = scout_log.run;
    if (strength != SCOUT_LOG_NO_LEVEL) entry.strength = strength;
    if (index) memmove(scout_log.entries + 1, scout_log.entries, index * sizeof(scout_log.entries[0]));
    scout_log.entries[0] = entry;
    scout_log_dirty = true;
}

static void scout_log_lost(unsigned kind, unsigned mhz, int64_t now) {
    const unsigned index = scout_log_find(kind, mhz);
    if (index == scout_log.count) return;
    scout_log_entry_t *entry = &scout_log.entries[index];
    if (entry->seen_run != scout_log.run ||
        (entry->lost_run == scout_log.run && entry->lost_seconds >= entry->seen_seconds)) return;
    entry->lost_run = scout_log.run;
    entry->lost_seconds = now / 1000000;
    scout_log_dirty = scout_log_urgent = true;
}

static const char *scout_log_strength_unit(unsigned kind) {
    if (kind >= SCOUT_LOG_RID_WIFI) return "dBm";
    if (kind >= SCOUT_LOG_ELRS) return "dBFS";
    return "dB above floor";
}

static const char *scout_log_band(unsigned kind, unsigned mhz) {
    if (kind <= SCOUT_LOG_AVATAR) return "5.8 GHz";
    if (mhz >= 5000) return "5 GHz";
    if (mhz || kind != SCOUT_LOG_RID_WIFI) return "2.4 GHz";
    return "Wi-Fi";
}
