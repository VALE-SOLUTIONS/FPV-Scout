/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#define SCOUT_RID_CAPACITY 4
#define SCOUT_RID_EXPIRY_US 30000000LL

typedef struct {
    uint8_t source[6], transport, channel;
    int8_t rssi;
    char id[21];
} scout_rid_packet_t;

typedef struct {
    unsigned matches, ble_matches;
    uint8_t next_channel, channel;
    uint8_t latest_wifi_channel;
    int8_t latest_wifi_rssi, latest_ble_rssi;
    bool ble_enabled, ble_window;
    int64_t channel_until, window_until;
    struct {
        scout_rid_packet_t packet;
        int64_t seen;
    } aircraft[SCOUT_RID_CAPACITY];
} scout_rid_state_t;
/* Static data must stay below the RF capture bank. */
static scout_rid_state_t *scout_rid;

#ifdef SCOUT_RUST_CORE
#include "freertos/queue.h"
#include <stdatomic.h>
#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#endif
extern uint8_t rust_scout_rid_wifi(const uint8_t *, size_t, char *);
static QueueHandle_t scout_rid_inbox;
static atomic_uint scout_rid_wifi_frames;
static wifi_promiscuous_filter_t scout_rid_saved_filter;
static wifi_band_mode_t scout_rid_saved_band;
static bool scout_rid_wifi_saved;

#if CONFIG_BT_NIMBLE_ENABLED
extern bool rust_scout_rid_ble(const uint8_t *, size_t, char *);
static atomic_bool scout_rid_ble_ready;
static atomic_uint scout_rid_ble_reports;

static int scout_rid_ble_receive(struct ble_gap_event *event, void *arg) {
    (void)arg;
    const uint8_t *data, *address;
    size_t size;
    int8_t rssi;
    if (event->type == BLE_GAP_EVENT_EXT_DISC) {
        const struct ble_gap_ext_disc_desc *report = &event->ext_disc;
        if (report->data_status != BLE_GAP_EXT_ADV_DATA_STATUS_COMPLETE) return 0;
        data = report->data;
        size = report->length_data;
        address = report->addr.val;
        rssi = report->rssi;
    } else if (event->type == BLE_GAP_EVENT_DISC) {
        const struct ble_gap_disc_desc *report = &event->disc;
        data = report->data;
        size = report->length_data;
        address = report->addr.val;
        rssi = report->rssi;
    } else return 0;
    atomic_fetch_add_explicit(&scout_rid_ble_reports, 1, memory_order_relaxed);
    scout_rid_packet_t packet = {.transport = 3, .rssi = rssi};
    if (rust_scout_rid_ble(data, size, packet.id)) {
        memcpy(packet.source, address, sizeof(packet.source));
        (void)xQueueSend(scout_rid_inbox, &packet, 0);
    }
    return 0;
}

static void scout_rid_ble_synced(void) {
    atomic_store_explicit(&scout_rid_ble_ready, true, memory_order_release);
}

static void scout_rid_ble_task(void *arg) {
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static bool scout_rid_ble_init(void) {
    atomic_store_explicit(&scout_rid_ble_ready, false, memory_order_relaxed);
    atomic_store_explicit(&scout_rid_ble_reports, 0, memory_order_relaxed);
    if (nimble_port_init() != ESP_OK) return false;
    ble_hs_cfg.sync_cb = scout_rid_ble_synced;
    nimble_port_freertos_init(scout_rid_ble_task);
    return true;
}

static bool scout_rid_ble_listen(void) {
    if (!atomic_load_explicit(&scout_rid_ble_ready, memory_order_acquire)) return false;
    const struct ble_gap_ext_disc_params passive = {.itvl = 160, .window = 160, .passive = 1};
    return ble_gap_ext_disc(BLE_OWN_ADDR_PUBLIC, 0, 0, 0, 0, 0, &passive, &passive,
                            scout_rid_ble_receive, NULL) == 0;
}

static void scout_rid_ble_pause(void) {
    if (ble_gap_disc_active()) (void)ble_gap_disc_cancel();
}

static void scout_rid_ble_close(void) {
    scout_rid_ble_pause();
    if (nimble_port_stop() == 0) (void)nimble_port_deinit();
}

static unsigned scout_rid_ble_frames(void) {
    return atomic_load_explicit(&scout_rid_ble_reports, memory_order_relaxed);
}
#else
static bool scout_rid_ble_init(void) { return false; }
static bool scout_rid_ble_listen(void) { return false; }
static void scout_rid_ble_pause(void) {}
static void scout_rid_ble_close(void) {}
static unsigned scout_rid_ble_frames(void) { return 0; }
#endif

static void scout_rid_wifi_receive(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *frame = buf;
    if (frame->rx_ctrl.rx_state || frame->rx_ctrl.sig_len < 28) return;
    atomic_fetch_add_explicit(&scout_rid_wifi_frames, 1, memory_order_relaxed);
    scout_rid_packet_t packet = {0};
    packet.transport = rust_scout_rid_wifi(frame->payload, frame->rx_ctrl.sig_len - 4, packet.id);
    if (!packet.transport) return;
    memcpy(packet.source, frame->payload + 10, sizeof(packet.source));
    packet.rssi = frame->rx_ctrl.rssi;
    packet.channel = frame->rx_ctrl.channel;
    (void)xQueueSend(scout_rid_inbox, &packet, 0);
}

static void scout_rid_stop(void) {
    if (scout_rid_wifi_saved) {
        (void)esp_wifi_set_promiscuous(false);
        if (scout_rid && scout_rid->ble_enabled) scout_rid_ble_close();
        (void)esp_wifi_set_promiscuous_rx_cb(NULL);
        (void)esp_wifi_set_promiscuous_filter(&scout_rid_saved_filter);
        (void)esp_wifi_set_band_mode(scout_rid_saved_band);
        (void)esp_wifi_set_promiscuous(true);
        scout_rid_wifi_saved = false;
        rx_ready = false;
    }
    free(scout_rid);
    scout_rid = NULL;
}

static void scout_rid_start(void) {
    scout_rid = heap_caps_calloc(1, sizeof(*scout_rid), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!scout_rid) return;
    if (!scout_rid_inbox) scout_rid_inbox = xQueueCreate(12, sizeof(scout_rid_packet_t));
    if (!scout_rid_inbox || esp_wifi_get_promiscuous_filter(&scout_rid_saved_filter) ||
        esp_wifi_get_band_mode(&scout_rid_saved_band)) {
        scout_rid_stop();
        return;
    }
    xQueueReset(scout_rid_inbox);
    atomic_store_explicit(&scout_rid_wifi_frames, 0, memory_order_relaxed);
    scout_rid_wifi_saved = true;
    const wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    if (esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO) ||
        esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE) ||
        esp_wifi_set_promiscuous_filter(&filter) ||
        esp_wifi_set_promiscuous_rx_cb(scout_rid_wifi_receive)) {
        scout_rid_stop();
        return;
    }
    scout_rid->ble_enabled = scout_rid_ble_init();
    scout_rid->window_until = esp_timer_get_time() + 2000000;
}

static void scout_rid_record(scout_rid_packet_t packet, int64_t now) {
    unsigned slot = 0;
    for (unsigned i = 0; i < SCOUT_RID_CAPACITY; ++i) {
        const scout_rid_packet_t *previous = &scout_rid->aircraft[i].packet;
        if (scout_rid->aircraft[i].seen && now - scout_rid->aircraft[i].seen < SCOUT_RID_EXPIRY_US &&
            (((previous->transport == 3) == (packet.transport == 3) &&
              !memcmp(previous->source, packet.source, sizeof(packet.source))) ||
             (previous->id[0] && !strcmp(previous->id, packet.id)))) {
            slot = i;
            if (!packet.id[0]) memcpy(packet.id, previous->id, sizeof(packet.id));
            break;
        }
        if (scout_rid->aircraft[i].seen < scout_rid->aircraft[slot].seen) slot = i;
    }
    scout_rid->aircraft[slot].packet = packet;
    scout_rid->aircraft[slot].seen = now;
    scout_rid->matches++;
    if (packet.transport == 3) {
        scout_rid->ble_matches++;
        scout_rid->latest_ble_rssi = packet.rssi;
    } else {
        scout_rid->latest_wifi_rssi = packet.rssi;
        scout_rid->latest_wifi_channel = packet.channel;
    }
}

static void scout_rid_poll(int64_t now) {
    if (!scout_rid) return;
    static const uint8_t channels[] = {6,149,1,11,6,149,36,44,6,149,153,157,
                                       6,149,161,165,2,3,4,5,7,8,9,10,12,13,40,48};
    /* The C5 has one RF path. Give Wi-Fi and Bluetooth dedicated listening windows. */
    if (scout_rid->ble_enabled && now >= scout_rid->window_until) {
        if (scout_rid->ble_window) {
            scout_rid_ble_pause();
            (void)esp_wifi_set_promiscuous(true);
            scout_rid->ble_window = false;
            scout_rid->channel_until = 0;
        } else if (esp_wifi_set_promiscuous(false) == ESP_OK) {
            scout_rid->ble_window = scout_rid_ble_listen();
            if (!scout_rid->ble_window) (void)esp_wifi_set_promiscuous(true);
        }
        scout_rid->window_until = now + 2000000;
    }
    if (!scout_rid->ble_window && now >= scout_rid->channel_until) {
        for (unsigned i = 0; i < sizeof(channels); ++i) {
            const uint8_t channel = channels[scout_rid->next_channel];
            scout_rid->next_channel = (scout_rid->next_channel + 1) % sizeof(channels);
            if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) continue;
            scout_rid->channel = channel;
            scout_rid->channel_until = now + (channel == 6 || channel == 149 ? 477000 : 231000);
            break;
        }
        if (now >= scout_rid->channel_until) {
            scout_rid_stop();
            return;
        }
    }
    scout_rid_packet_t packet;
    for (unsigned i = 0; i < 12 && xQueueReceive(scout_rid_inbox, &packet, 0); ++i)
        scout_rid_record(packet, now);
}

static unsigned scout_rid_frames(void) {
    return atomic_load_explicit(&scout_rid_wifi_frames, memory_order_relaxed);
}
#else
static void scout_rid_start(void) {}
static void scout_rid_stop(void) {}
static void scout_rid_poll(int64_t now) { (void)now; }
static unsigned scout_rid_frames(void) { return 0; }
static unsigned scout_rid_ble_frames(void) { return 0; }
#endif
