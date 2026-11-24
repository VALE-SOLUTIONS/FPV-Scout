/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cyd_c5.h"
#include "cyd_wallpaper.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef CONFIG_CYD_C5_TOUCH
#define CONFIG_CYD_C5_TOUCH 0
#endif

enum { MAX_FILE = 2 * 1024 * 1024, MAX_IMAGE_ALLOC = 4 * 1024 * 1024 };
static const unsigned image_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

static void *image_realloc(void *ptr, size_t bytes) {
    return bytes <= MAX_IMAGE_ALLOC ? heap_caps_realloc(ptr, bytes, image_caps) : NULL;
}

#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 640
#define STBI_MALLOC(n) image_realloc(NULL, (n))
#define STBI_REALLOC(p,n) image_realloc((p), (n))
#define STBI_FREE(p) free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

typedef struct {
    stbi__gif raster;
    stbi__context input;
} gif_decoder_t;

static struct {
    uint8_t *pixels, *file, *saved;
    int width, height, length;
    int64_t next_frame;
    bool saved_valid;
    gif_decoder_t *gif;
} image;
static char status[64] = "Wallpaper not read";
static uint8_t display_mode;
enum { WALLPAPER_NAME_SIZE = sizeof(((struct dirent *)0)->d_name) };
static char *selected_name;

void cyd_wallpaper_init(void) {
    nvs_handle_t nvs;
    uint8_t saved;
    if (!selected_name) selected_name = heap_caps_calloc(WALLPAPER_NAME_SIZE, 1, image_caps);
    if (nvs_open("cyd", NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_u8(nvs, "wallpaper_mode", &saved) == ESP_OK && saved <= CYD_WALLPAPER_OFF)
            display_mode = saved;
        size_t length = WALLPAPER_NAME_SIZE;
        if (selected_name && nvs_get_str(nvs, "wallpaper_file", selected_name, &length) != ESP_OK)
            selected_name[0] = '\0';
        nvs_close(nvs);
    }
    cyd_wallpaper_reload();
}

cyd_wallpaper_mode_t cyd_wallpaper_mode(void) { return display_mode; }

void cyd_wallpaper_next_mode(void) {
    display_mode = (display_mode + 1) % (CYD_WALLPAPER_OFF + 1);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cyd", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, "wallpaper_mode", display_mode);
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (err != ESP_OK) printf("SCOUT wallpaper mode save failed: %s\n", esp_err_to_name(err));
}

static void clear_image(void) {
    if (image.gif) {
        free(image.gif->raster.out);
        free(image.gif);
    } else free(image.pixels);
    free(image.saved);
    free(image.file);
    memset(&image, 0, sizeof(image));
}

/* The pinned stb decoder retains the current canvas. Keep only that canvas and
 * the last restore point in memory, regardless of the number of GIF frames. */
static bool gif_next(void) {
    int channels;
    stbi__gif *gif = &image.gif->raster;
    stbi__context *input = &image.gif->input;
    uint8_t *frame = stbi__gif_load_next(input, gif, &channels, 4,
                                      image.saved_valid ? image.saved : NULL);
    if (frame == (uint8_t *)input) {
        free(gif->out);
        memset(gif, 0, sizeof(*gif));
        image.saved_valid = false;
        stbi__start_mem(input, image.file, image.length);
        frame = stbi__gif_load_next(input, gif, &channels, 4, NULL);
    }
    if (!frame || frame == (uint8_t *)input) return false;
    image.pixels = frame;
    image.width = gif->w;
    image.height = gif->h;
    if ((gif->eflags & 0x1c) != 0x0c) {
        size_t bytes = (size_t)image.width * image.height * 4;
        if (!image.saved) image.saved = STBI_MALLOC(bytes);
        if (!image.saved) return false;
        memcpy(image.saved, frame, bytes);
        image.saved_valid = true;
    }
    return true;
}

static bool load_file(const char *path, bool gif) {
    const char *kind = gif ? "GIF" : "PNG";
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errno != ENOENT) snprintf(status, sizeof(status), "%s open error %d", kind, errno);
        return false;
    }
    uint8_t *data = NULL;
    long n = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
    if (n > MAX_FILE) snprintf(status, sizeof(status), "%s exceeds 2 MiB", kind);
    else if (n <= 0 || fseek(f, 0, SEEK_SET) != 0)
        snprintf(status, sizeof(status), "%s empty or unreadable", kind);
    else {
        data = STBI_MALLOC((size_t)n);
        if (!data) snprintf(status, sizeof(status), "No memory for %s", kind);
        else if (fread(data, 1, (size_t)n, f) != (size_t)n) {
            snprintf(status, sizeof(status), "%s read failed", kind);
            free(data);
            data = NULL;
        }
    }
    fclose(f);
    if (!data) return false;

    bool loaded = false;
    if (gif) {
        image.file = data;
        image.length = (int)n;
        image.gif = heap_caps_calloc(1, sizeof(*image.gif), image_caps);
        if (image.gif) {
            stbi__start_mem(&image.gif->input, data, (int)n);
            loaded = gif_next();
        }
    } else {
        static const uint8_t png_magic[] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
        int channels;
        if (n >= (long)sizeof(png_magic) && !memcmp(data, png_magic, sizeof(png_magic)))
            image.pixels = stbi_load_from_memory(data, (int)n, &image.width, &image.height, &channels, 4);
        free(data);
        loaded = image.pixels != NULL;
    }
    if (loaded) {
        snprintf(status, sizeof(status), "%s loaded %d x %d", kind, image.width, image.height);
        return true;
    }
    snprintf(status, sizeof(status), "%s decode failed (max 640 px)", kind);
    clear_image();
    return false;
}

static int wallpaper_kind(const char *name) {
    if (name[0] == '.') return 0;
    const char *suffix = strrchr(name, '.');
    if (suffix && !strcasecmp(suffix, ".png")) return 1;
    return suffix && !strcasecmp(suffix, ".gif") ? 2 : 0;
}

static bool load_folder(const char *folder, bool advance) {
    DIR *dir = opendir(folder);
    if (!dir) return false;
    char first[WALLPAPER_NAME_SIZE] = "", next[WALLPAPER_NAME_SIZE] = "";
    for (struct dirent *entry; (entry = readdir(dir));) {
        const char *name = entry->d_name;
        if (entry->d_type == DT_DIR || !wallpaper_kind(name)) continue;
        if (!advance && !strcasecmp(name, selected_name)) {
            strcpy(next, name);
            break;
        }
        if (!first[0] || strcasecmp(name, first) < 0) strcpy(first, name);
        if (advance && strcasecmp(name, selected_name) > 0 &&
            (!next[0] || strcasecmp(name, next) < 0)) strcpy(next, name);
    }
    closedir(dir);
    const char *name = next[0] ? next : first;
    if (!name[0]) return false;
    strcpy(selected_name, name);
    char path[WALLPAPER_NAME_SIZE + 24];
    snprintf(path, sizeof(path), "%s/%s", folder, name);
    if (load_file(path, wallpaper_kind(name) == 2)) {
        snprintf(status, sizeof(status), "%.47s", name);
        if (advance) {
            nvs_handle_t nvs;
            esp_err_t err = nvs_open("cyd", NVS_READWRITE, &nvs);
            if (err == ESP_OK) {
                err = nvs_set_str(nvs, "wallpaper_file", name);
                if (err == ESP_OK) err = nvs_commit(nvs);
                nvs_close(nvs);
            }
            if (err != ESP_OK) printf("SCOUT wallpaper file save failed: %s\n", esp_err_to_name(err));
        }
    }
    return true;
}

esp_err_t cyd_sd_access(esp_err_t (*action)(const char *, void *), void *context) {
    const char *path = "/scoutsd";
    sdmmc_card_t *card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = SPI2_HOST;
    slot.gpio_cs = CYD_PIN_SD_CS;
    const esp_vfs_fat_mount_config_t config = { .max_files = 1 };
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_WARN);
    esp_log_level_set("sdmmc_init", ESP_LOG_ERROR);
    esp_err_t err = esp_vfs_fat_sdspi_mount(path, &host, &slot, &config, &card);
    esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_NONE);
    esp_log_level_set("sdmmc_init", ESP_LOG_NONE);
    if (err == ESP_OK) {
        err = action(path, context);
        const esp_err_t unmount = esp_vfs_fat_sdcard_unmount(path, card);
        if (err == ESP_OK) err = unmount;
    }
    gpio_set_direction(CYD_PIN_SD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(CYD_PIN_SD_CS, 1);
    return err;
}

static esp_err_t load_wallpaper_files(const char *root, void *context) {
    (void)root;
    const bool advance = *(const bool *)context;
    snprintf(status, sizeof(status), "No wallpaper.png or .gif");
    bool found = selected_name && (load_folder("/scoutsd/wallpapers", advance) ||
                                   load_folder("/scoutsd/walpapers", advance));
    if (!found && !load_file("/scoutsd/wallpaper.png", false)) load_file("/scoutsd/wallpaper.gif", true);
    return ESP_OK;
}

static void load_wallpaper(bool advance) {
    clear_image();
    if (!CONFIG_CYD_C5_TOUCH || !heap_caps_get_total_size(image_caps)) {
        snprintf(status, sizeof(status), "Wallpaper memory unavailable");
        return;
    }
    const esp_err_t err = cyd_sd_access(load_wallpaper_files, &advance);
    if (err != ESP_OK) snprintf(status, sizeof(status), "SD mount: %s", esp_err_to_name(err));
}

void cyd_wallpaper_reload(void) { load_wallpaper(false); }

void cyd_wallpaper_next_file(void) { load_wallpaper(true); }

const char *cyd_wallpaper_status(void) { return status; }

bool cyd_wallpaper_active(void) { return image.pixels != NULL; }

bool cyd_wallpaper_update(int64_t now) {
    if (!image.gif || now < image.next_frame) return false;
    if (image.next_frame && !gif_next()) {
        clear_image();
        snprintf(status, sizeof(status), "GIF animation decode failed");
        return true;
    }
    int delay = image.gif->raster.delay;
    image.next_frame = now + (int64_t)(delay < 100 ? 100 : delay) * 1000;
    return true;
}

void cyd_wallpaper_row(uint16_t *pixels, int width, int height, int y) {
    int sw = image.width, sh = image.height;
    if (sw * height > sh * width) sw = sh * width / height;
    else sh = sw * height / width;
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;
    const uint8_t *row = image.pixels +
        4 * (((image.height - sh) / 2 + y * sh / height) * image.width + (image.width - sw) / 2);
    for (int x = 0; x < width; x++) {
        const uint8_t *rgba = row + 4 * (x * sw / width);
        unsigned a = rgba[3] * 150u / 255u;
        pixels[x] = cyd_rgb((rgba[0] * a + 10 * (255 - a)) / 255,
                            (rgba[1] * a + 14 * (255 - a)) / 255,
                            (rgba[2] * a + 20 * (255 - a)) / 255);
    }
}
