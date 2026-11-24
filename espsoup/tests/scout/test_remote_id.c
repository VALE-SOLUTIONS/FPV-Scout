/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern uint8_t rust_scout_rid_wifi(const uint8_t *, size_t, char *);
extern bool rust_scout_rid_ble(const uint8_t *, size_t, char *);

int main(void) {
    const uint8_t pack[28] = {0xf2,25,1,0x02,0x12,'T','E','S','T','-','R','I','D'};
    uint8_t beacon[71] = {0x80};
    const uint8_t vendor[] = {221,33,0xfa,0x0b,0xbc,0x0d,0};
    memcpy(beacon+36,vendor,sizeof(vendor));
    memcpy(beacon+43,pack,sizeof(pack));
    char id[21];
    assert(rust_scout_rid_wifi(beacon,sizeof(beacon),id)==1 && !strcmp(id,"TEST-RID"));
    for (size_t i=0;i<sizeof(beacon);++i) assert(!rust_scout_rid_wifi(beacon,i,id));
    beacon[45]=9;
    assert(!rust_scout_rid_wifi(beacon,sizeof(beacon),id));
    beacon[45]=1;
    beacon[41]=0;
    assert(!rust_scout_rid_wifi(beacon,sizeof(beacon),id));

    uint8_t nan[72] = {0xd0};
    const uint8_t header[] = {0x04,0x09,0x50,0x6f,0x9a,0x13,3,39,0,
                              0x88,0x69,0x19,0x9d,0x92,0x09,1,0,0x10,29,0};
    memcpy(nan+24,header,sizeof(header));
    memcpy(nan+44,pack,sizeof(pack));
    assert(rust_scout_rid_wifi(nan,sizeof(nan),id)==2 && !strcmp(id,"TEST-RID"));
    nan[42]=28;
    assert(!rust_scout_rid_wifi(nan,sizeof(nan),id));
    nan[42]=29;
    nan[47]=0x12; /* A valid Location message without Basic ID. */
    assert(rust_scout_rid_wifi(nan,sizeof(nan),id)==2 && id[0]==0);

    uint8_t legacy[31] = {30,0x16,0xfa,0xff,0x0d,7};
    memcpy(legacy+6,pack+3,25);
    assert(rust_scout_rid_ble(legacy,sizeof(legacy),id) && !strcmp(id,"TEST-RID"));
    for (size_t i=0;i<sizeof(legacy);++i) assert(!rust_scout_rid_ble(legacy,i,id));
    legacy[4]=0x0e;
    assert(!rust_scout_rid_ble(legacy,sizeof(legacy),id));

    uint8_t extended[37] = {2,1,6,33,0x16,0xfa,0xff,0x0d,8};
    memcpy(extended+9,pack,sizeof(pack));
    assert(rust_scout_rid_ble(extended,sizeof(extended),id) && !strcmp(id,"TEST-RID"));
    extended[11]=2;
    assert(!rust_scout_rid_ble(extended,sizeof(extended),id));
    puts("PASS: Wi-Fi Beacon, Wi-Fi NAN, legacy and extended Bluetooth Remote ID, and malformed input rejection");
}
