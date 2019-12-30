#ifndef SDCARD_INFO_H
#define SDCARD_INFO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((__packed__)) {

    uint32_t serial;
    uint16_t date_year;
    uint8_t date_month;
    union {
        uint8_t version;
        struct {
            uint8_t version_minor:4;
            uint8_t version_major:4;
        };
    };

    uint8_t manufacturer;
    char oem_id[3];
    char name[6];

    bool valid;

} SDInfo;

SDInfo sdcard_info(void);

#endif

