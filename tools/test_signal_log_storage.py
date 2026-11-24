#!/usr/bin/env python3
"""Run the actual Signal Log storage code with a fake NVS and a temporary SD directory."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / 'espsoup'
source = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
static int test_open(const char *path,int flags,int mode){
    (void)mode;return _open(path,flags|_O_BINARY,_S_IREAD|_S_IWRITE);
}
#define open test_open
#define close _close
#define fdopen _fdopen
#define fsync _commit
#define unlink _unlink
#endif
#include "scout_log_storage.h"

static scout_log_t stored, pending;
static bool present, nvs_ok=true, sd_ok=true;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){(void)mode;assert(!strcmp(name,"cyd"));*h=1;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t len){
    assert(h==1 && !strcmp(key,"signal_log_v1") && len==sizeof(pending));
    memcpy(&pending,value,len);return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *value,size_t *len){
    assert(h==1 && !strcmp(key,"signal_log_v1") && *len==sizeof(stored));
    if(!present)return ESP_FAIL;
    memcpy(value,&stored,*len);return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);if(!nvs_ok)return ESP_FAIL;stored=pending;present=true;return ESP_OK;}
esp_err_t cyd_sd_access(esp_err_t (*action)(const char *,void *),void *ctx){return sd_ok?action(".",ctx):ESP_FAIL;}
int main(void){
    char status[40], line[180];
    scout_log_record(SCOUT_LOG_ANALOG,5769,44,1000000);
    scout_log_checkpoint(1000000);
    scout_log_record(SCOUT_LOG_ANALOG,5769,45,2000000);
    scout_log_lost(SCOUT_LOG_ANALOG,5769,5000000);
    scout_log_checkpoint(5000000);
    scout_log=(scout_log_t){.run=1};
    scout_log_load();
    assert(scout_log.run==2 && scout_log.count==1);
    assert(scout_log.entries[0].first_seconds==1 && scout_log.entries[0].seen_seconds==2);
    assert(scout_log.entries[0].lost_seconds==5 && scout_log.entries[0].strength==45);
    sd_ok=false;
    scout_log_save_to_sd(status,sizeof(status));
    assert(strstr(status,"Save failed") && scout_log.count==1 && stored.count==1);
    sd_ok=true;
    FILE *file=fopen("SC000001.CSV","wb");assert(file);fputs("keep me",file);assert(!fclose(file));
    nvs_ok=false;
    assert(!scout_log_clear() && scout_log.count==1);
    scout_log_save_to_sd(status,sizeof(status));
    assert(strstr(status,"Export saved") && scout_log.count==1 && stored.count==1);
    nvs_ok=true;
    scout_log_save_to_sd(status,sizeof(status));
    assert(!strcmp(status,"Saved SC000003.CSV") && !scout_log.count && !stored.count);
    file=fopen("SC000001.CSV","rb");assert(file && fgets(line,sizeof(line),file) && !strcmp(line,"keep me"));fclose(file);
    file=fopen("SC000003.CSV","rb");assert(file && fgets(line,sizeof(line),file));
    assert(fgets(line,sizeof(line),file) && !strcmp(line,"Analog video,5.8 GHz,5769,45,dB above floor,1,1,1,2,1,5\r\n"));
    assert(fgetc(file)==EOF);fclose(file);
    puts("PASS: reboot persistence, first/last/lost times, SD failure, archive collision, and clear only after verified export");
}
'''

with tempfile.TemporaryDirectory(prefix='scout-log-') as temp:
    folder = Path(temp)
    (folder / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL (-1)\n')
    (folder / 'nvs.h').write_text('''#pragma once
#include <stddef.h>
#include "esp_err.h"
typedef int nvs_handle_t;
enum {NVS_READONLY,NVS_READWRITE};
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_commit(nvs_handle_t);
''')
    if os.name == 'nt':
        (folder / 'unistd.h').write_text('#include <io.h>\n')
    (folder / 'test.c').write_text(source)
    includes = [folder, root / 'main/families/c5_c6_c61', root / 'components/cyd_c5/include']
    if cl := shutil.which('cl'):
        exe = folder / 'test.exe'
        command = [cl, '/nologo', '/std:c11', '/O2', *['/I' + str(p) for p in includes], 'test.c', '/Fe:' + str(exe)]
    else:
        exe = folder / 'test'
        cc = shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
        if not cc:
            raise SystemExit('Activate a host C compiler')
        command = [cc, '-std=gnu11', '-O2', *['-I' + str(p) for p in includes], 'test.c', '-o', str(exe)]
    subprocess.run(command, cwd=folder, check=True)
    subprocess.run([str(exe)], cwd=folder, check=True)
