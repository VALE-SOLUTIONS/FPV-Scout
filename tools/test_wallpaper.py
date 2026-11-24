#!/usr/bin/env python3
"""Exercise the device wallpaper decoder, animation deadlines and RGB565 rows."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COMP = ROOT / "espsoup/components/cyd_c5"

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *sample_png, *sample_gif, *sample_broken;
static int mounted_card;
static FILE *card_fopen(const char *path, const char *mode) {
    if (path[0]!='/' || strncmp(path,"/scoutsd/",9)) return fopen(path,mode);
    const char *name=strrchr(path,'/')+1;
    if (!strcmp(name,"Alpha.PNG") || !strcmp(name,"wallpaper.png")) return fopen(sample_png,mode);
    if (!strcmp(name,"Beta.gif") || !strcmp(name,"Zulu.GIF")) return fopen(sample_gif,mode);
    return fopen(sample_broken,mode);
}
#define fopen card_fopen
#include "cyd_wallpaper.c"
#undef fopen

int main(int argc, char **argv) {
    assert(argc == 4);
    sample_png=argv[1];sample_gif=argv[2];sample_broken=argv[3];
    uint16_t row[320];
    cyd_wallpaper_reload();
    assert(!cyd_wallpaper_active());
    assert(!strcmp(cyd_wallpaper_status(), "SD mount: ESP_FAIL"));
    assert(cyd_wallpaper_mode() == CYD_WALLPAPER_MAIN);
    cyd_wallpaper_next_mode();
    assert(cyd_wallpaper_mode() == CYD_WALLPAPER_ALL && saved_mode == CYD_WALLPAPER_ALL);
    display_mode = CYD_WALLPAPER_MAIN;
    cyd_wallpaper_init();
    assert(cyd_wallpaper_mode() == CYD_WALLPAPER_ALL);
    cyd_wallpaper_next_mode();
    assert(cyd_wallpaper_mode() == CYD_WALLPAPER_OFF && saved_mode == CYD_WALLPAPER_OFF);
    cyd_wallpaper_next_mode();
    assert(cyd_wallpaper_mode() == CYD_WALLPAPER_MAIN && saved_mode == CYD_WALLPAPER_MAIN);
    saved_mode = 255;
    cyd_wallpaper_init();
    assert(cyd_wallpaper_mode() == CYD_WALLPAPER_MAIN);
    assert(!load_file(argv[3], false));
    assert(!load_file(argv[3], true));
    assert(!cyd_wallpaper_active());

    assert(load_file(argv[1], false));
    assert(cyd_wallpaper_active() && !cyd_wallpaper_update(100));
    assert(!strcmp(cyd_wallpaper_status(), "PNG loaded 40 x 30"));
    cyd_wallpaper_row(row, 240, 320, 20);
    assert(row[0] != row[239] && row[239] == CYD_RGB(10,14,20));
    cyd_wallpaper_row(row, 320, 240, 239);
    assert(row[0] != row[319] && row[319] == CYD_RGB(10,14,20));
    clear_image();

    assert(load_file(argv[2], true));
    assert(cyd_wallpaper_active() && cyd_wallpaper_update(100));
    assert(!strcmp(cyd_wallpaper_status(), "GIF loaded 320 x 240"));
    cyd_wallpaper_row(row, 320, 240, 0);
    uint16_t first = row[0];
    assert(!cyd_wallpaper_update(100099));
    assert(cyd_wallpaper_update(100100));
    cyd_wallpaper_row(row, 320, 240, 0);
    uint16_t second = row[0];
    assert(second != first);
    assert(!cyd_wallpaper_update(250099));
    assert(cyd_wallpaper_update(250100));
    cyd_wallpaper_row(row, 320, 240, 0);
    assert(row[0] != first && row[0] != second);
    assert(cyd_wallpaper_update(450100));
    cyd_wallpaper_row(row, 320, 240, 0);
    assert(row[0] == first);
    cyd_wallpaper_reload();
    assert(!cyd_wallpaper_active());
    mounted_card=1;
    cyd_wallpaper_reload();
    assert(cyd_wallpaper_active() && !strcmp(cyd_wallpaper_status(),"Alpha.PNG"));
    cyd_wallpaper_next_file();
    assert(cyd_wallpaper_active() && !strcmp(cyd_wallpaper_status(),"Beta.gif") && !strcmp(saved_file,"Beta.gif"));
    cyd_wallpaper_next_file();
    assert(!cyd_wallpaper_active() && strstr(cyd_wallpaper_status(),"decode failed") && !strcmp(saved_file,"Beta.gif"));
    cyd_wallpaper_next_file();
    assert(!strcmp(cyd_wallpaper_status(),"Zulu.GIF"));
    cyd_wallpaper_next_file();
    assert(!strcmp(cyd_wallpaper_status(),"Alpha.PNG"));
    cyd_wallpaper_next_file();
    selected_name[0]=0;
    cyd_wallpaper_init();assert(!strcmp(cyd_wallpaper_status(),"Beta.gif"));
    mounted_card=2;
    cyd_wallpaper_next_file();assert(strstr(cyd_wallpaper_status(),"decode failed")); /* Legacy folder spelling. */
    mounted_card=3;
    cyd_wallpaper_reload();assert(cyd_wallpaper_active() && !strcmp(cyd_wallpaper_status(),"PNG loaded 40 x 30"));
    puts("PASS: wallpaper cycling, malformed image recovery, saved selection, root fallback, GIF timing, and both orientations");
}
'''

STUBS = {
    "sdkconfig.h": "#define CONFIG_CYD_C5_TOUCH 1\n",
    "esp_err.h": """#pragma once
typedef int esp_err_t;
#define ESP_OK 0
static const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_FAIL"; }
""",
    "driver/gpio.h": """#define GPIO_MODE_OUTPUT 1
static void gpio_set_direction(int pin, int mode) { (void)pin; (void)mode; }
static void gpio_set_level(int pin, int level) { (void)pin; (void)level; }
""",
    "driver/sdspi_host.h": """#define SPI2_HOST 1
typedef struct { int slot, max_freq_khz; } sdmmc_host_t;
typedef struct { int host_id, gpio_cs; } sdspi_device_config_t;
#define SDSPI_HOST_DEFAULT() {0}
#define SDSPI_DEVICE_CONFIG_DEFAULT() {0}
""",
    "esp_heap_caps.h": """#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define heap_caps_get_total_size(c) ((void)(c), 8 * 1024 * 1024)
#define heap_caps_realloc(p,n,c) ((void)(c), realloc(p,n))
#define heap_caps_calloc(i,n,c) ((void)(c), calloc(i,n))
""",
    "dirent.h": """#pragma once
#include <string.h>
enum { DT_DIR=4, DT_REG=8 };
struct dirent { unsigned char d_type; char d_name[256]; };
typedef struct { unsigned index; } DIR;
static DIR *opendir(const char *path) {
    static DIR dir;
    dir.index=0;
    if(mounted_card==1 && !strcmp(path,"/scoutsd/wallpapers")) return &dir;
    if(mounted_card==2 && !strcmp(path,"/scoutsd/walpapers")) return &dir;
    return NULL;
}
static struct dirent *readdir(DIR *dir) {
    static const char *const names[]={"Zulu.GIF",".Hidden.png","Dir.png","Note.txt","Broken.png","Beta.gif","Alpha.PNG"};
    static struct dirent item;
    unsigned i=dir->index++;
    if (i>=sizeof(names)/sizeof(names[0])) return NULL;
    item.d_type=i==2?DT_DIR:DT_REG;
    strcpy(item.d_name,names[i]);
    return &item;
}
static int closedir(DIR *dir){(void)dir;return 0;}
""",
    "strings.h": """#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include_next <strings.h>
#endif
""",
    "esp_log.h": """#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
static void esp_log_level_set(const char *tag, int level) { (void)tag; (void)level; }
""",
    "nvs.h": """typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
static uint8_t saved_mode;
static char saved_file[256];
static esp_err_t nvs_open(const char *ns, int access, nvs_handle_t *nvs) {
    (void)ns; (void)access; *nvs = 1; return ESP_OK;
}
static esp_err_t nvs_get_u8(nvs_handle_t nvs, const char *key, uint8_t *value) {
    (void)nvs; (void)key; *value = saved_mode; return ESP_OK;
}
static esp_err_t nvs_set_u8(nvs_handle_t nvs, const char *key, uint8_t value) {
    (void)nvs; (void)key; saved_mode = value; return ESP_OK;
}
static esp_err_t nvs_get_str(nvs_handle_t h,const char *key,char *dst,size_t *len) {
    assert(h==1 && !strcmp(key,"wallpaper_file"));
    if (!saved_file[0] || strlen(saved_file)+1>*len) return -1;
    strcpy(dst,saved_file);return ESP_OK;
}
static esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *src) {
    assert(h==1 && !strcmp(key,"wallpaper_file"));strcpy(saved_file,src);return ESP_OK;
}
static esp_err_t nvs_commit(nvs_handle_t nvs) { (void)nvs; return ESP_OK; }
static void nvs_close(nvs_handle_t nvs) { (void)nvs; }
""",
    "esp_vfs_fat.h": """typedef struct { int max_files; } esp_vfs_fat_mount_config_t;
typedef struct { int unused; } sdmmc_card_t;
static int esp_vfs_fat_sdspi_mount(const char *p, const sdmmc_host_t *h,
const sdspi_device_config_t *s, const esp_vfs_fat_mount_config_t *m, sdmmc_card_t **c) {
    (void)p; (void)h; (void)s; (void)m; *c=NULL; return mounted_card?ESP_OK:-1;
}
static esp_err_t esp_vfs_fat_sdcard_unmount(const char *p, sdmmc_card_t *c) {
    (void)p;
    (void)c;
    return ESP_OK;
}
""",
}

with tempfile.TemporaryDirectory(prefix="scout-wallpaper-") as d:
    temp = Path(d)
    for name, content in STUBS.items():
        path = temp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (temp / "test.c").write_text(HARNESS)
    png = temp / "wallpaper.png"
    gif = temp / "wallpaper.gif"
    broken = temp / "broken"
    broken.write_bytes(b"broken wallpaper")
    still = Image.new("RGBA", (40, 30))
    still.paste((255, 0, 0, 255), (0, 0, 20, 30))
    still.save(png)
    frames = [Image.new("RGB", (320, 240), color) for color in ["red", "green", "blue"]]
    frames[0].save(gif, save_all=True, append_images=frames[1:], duration=[40, 150, 200], loop=0)
    includes = [temp, COMP, COMP / "include"]
    if cl := shutil.which("cl"):
        exe = temp / "test.exe"
        command = [cl, "/nologo", "/std:c11", "/O2", *["/I" + str(p) for p in includes],
                   str(temp / "test.c"), "/Fe:" + str(exe), "/Fo:" + str(temp) + os.sep]
    else:
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not cc:
            raise SystemExit("Activate a host C compiler before testing wallpaper")
        exe = temp / "test"
        command = [cc, "-std=c11", "-O2", *["-I" + str(p) for p in includes],
                   str(temp / "test.c"), "-o", str(exe)]
    subprocess.run(command, check=True)
    subprocess.run([exe, png, gif, broken], check=True)
