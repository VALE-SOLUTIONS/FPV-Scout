/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "nvs.h"
#include "cyd_wallpaper.h"
#include "scout_signal_log.h"

static bool scout_log_fault;
static int64_t scout_log_flush_at;

static bool scout_log_store(const scout_log_t *data) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cyd", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs, "signal_log_v1", data, sizeof(*data));
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    scout_log_fault = err != ESP_OK;
    return !scout_log_fault;
}

static void scout_log_load(void) {
    nvs_handle_t nvs;
    if (nvs_open("cyd", NVS_READONLY, &nvs) != ESP_OK) return;
    scout_log_t saved;
    size_t size = sizeof(saved);
    const esp_err_t err = nvs_get_blob(nvs, "signal_log_v1", &saved, &size);
    nvs_close(nvs);
    if (err != ESP_OK || size != sizeof(saved) || !saved.run || saved.count > SCOUT_LOG_CAPACITY) return;
    for (unsigned i = 0; i < saved.count; ++i)
        if (saved.entries[i].kind >= sizeof(scout_log_names) / sizeof(scout_log_names[0])) return;
    scout_log = saved;
    scout_log.run++;
    scout_log_dirty = scout_log.count != 0;
    scout_log_urgent = scout_log_dirty;
}

static void scout_log_checkpoint(int64_t now) {
    if (!scout_log_dirty || (now < scout_log_flush_at && (!scout_log_urgent || scout_log_fault))) return;
    const bool saved = scout_log_store(&scout_log);
    scout_log_dirty = !saved;
    scout_log_urgent = false;
    scout_log_flush_at = now + (saved ? 300000000 : 5000000);
}

static bool scout_log_clear(void) {
    const scout_log_t empty = {.run=scout_log.run};
    if (!scout_log_store(&empty)) return false;
    scout_log = empty;
    scout_log_dirty = scout_log_urgent = false;
    return true;
}

static bool scout_log_path_absent(const char *path) {
    struct stat info;
    errno = 0;
    return stat(path,&info) != 0 && errno == ENOENT;
}

static int scout_log_csv_row(char *line, size_t capacity, const scout_log_entry_t *entry) {
    char mhz[8] = "", level[16] = "", loss[30] = ",";
    if (entry->mhz) snprintf(mhz,sizeof(mhz),"%u",entry->mhz);
    if (entry->strength != SCOUT_LOG_NO_LEVEL) snprintf(level,sizeof(level),"%ld",(long)entry->strength);
    if (entry->lost_run) snprintf(loss,sizeof(loss),"%lu,%lu",(unsigned long)entry->lost_run,(unsigned long)entry->lost_seconds);
    return snprintf(line,capacity,"%s,%s,%s,%s,%s,%lu,%lu,%lu,%lu,%s\r\n",
        scout_log_names[entry->kind],scout_log_band(entry->kind,entry->mhz),mhz,level,
        level[0] ? scout_log_strength_unit(entry->kind) : "",(unsigned long)entry->first_run,
        (unsigned long)entry->first_seconds,(unsigned long)entry->seen_run,(unsigned long)entry->seen_seconds,loss);
}

static esp_err_t scout_log_export(const char *root, void *context) {
    static const char header[] = "Signal,Band,MHz,Strength,Unit,First boot,First seconds,Last boot,Last seconds,Lost boot,Lost seconds\r\n";
    char *name = context;
    char archive[40], temporary[40];
    unsigned file_number;
    for (file_number = 1; file_number <= 999999; ++file_number) {
        snprintf(archive,sizeof(archive),"%s/SC%06u.CSV",root,file_number);
        snprintf(temporary,sizeof(temporary),"%s/SC%06u.TMP",root,file_number);
        if (scout_log_path_absent(archive) && scout_log_path_absent(temporary)) break;
        if (errno != ENOENT && errno != 0) return ESP_FAIL;
    }
    if (file_number > 999999) return ESP_FAIL;
    const int fd = open(temporary,O_WRONLY | O_CREAT | O_EXCL,0666);
    if (fd < 0) return ESP_FAIL;
    FILE *file = fdopen(fd,"wb");
    bool ok = file && fputs(header,file) >= 0;
    char line[180], readback[180];
    for (unsigned i = 0; ok && i < scout_log.count; ++i) {
        const int len = scout_log_csv_row(line,sizeof(line),&scout_log.entries[i]);
        ok = len > 0 && len < sizeof(line) && fputs(line,file) >= 0;
    }
    if (file) {
        ok = ok && fflush(file) == 0 && fsync(fd) == 0;
        if (fclose(file)) ok = false;
    } else close(fd);
    if (ok) {
        file = fopen(temporary,"rb");
        ok = file && fgets(readback,sizeof(readback),file) && !strcmp(readback,header);
        for (unsigned i = 0; ok && i < scout_log.count; ++i) {
            scout_log_csv_row(line,sizeof(line),&scout_log.entries[i]);
            ok = fgets(readback,sizeof(readback),file) && !strcmp(readback,line);
        }
        if (file) {
            ok = ok && fgetc(file) == EOF && !ferror(file);
            if (fclose(file)) ok = false;
        }
    }
    if (ok && rename(temporary,archive) == 0) {
        snprintf(name,13,"SC%06u.CSV",file_number);
        return ESP_OK;
    }
    unlink(temporary);
    return ESP_FAIL;
}

static void scout_log_save_to_sd(char *status, size_t capacity) {
    char name[13] = "";
    if (cyd_sd_access(scout_log_export,name) != ESP_OK)
        snprintf(status,capacity,"Save failed. Log kept.");
    else if (!scout_log_clear())
        snprintf(status,capacity,"Export saved. Log kept.");
    else
        snprintf(status,capacity,"Saved %s",name);
}
