#include "sdcard_info.h"

#include <fatfs_lib/diskio.h>
#include <memory.h>

SDInfo sdcard_info(void)
{
    SDInfo info;
    memset(&info, 0, sizeof(info));

    uint32_t cid_buffer[4];
    if(RES_OK != disk_ioctl(0, MMC_GET_CID, cid_buffer)) {
        return info;
    }

    uint8_t *cid_bytes = (uint8_t*)cid_buffer;

    info.manufacturer = cid_bytes[15];

    info.oem_id[0] = cid_bytes[14];
    info.oem_id[1] = cid_bytes[13];
    info.oem_id[2] = '\0';

    info.name[0] = cid_bytes[12];
    info.name[1] = cid_bytes[11];
    info.name[2] = cid_bytes[10];
    info.name[3] = cid_bytes[9];
    info.name[4] = cid_bytes[8];
    info.name[5] = '\0';

    info.version = cid_bytes[7];
    info.serial = ((cid_bytes[6] << 24)
        | (cid_bytes[5] << 16)
        | (cid_bytes[4] << 8)
        | (cid_bytes[3]));

    const uint8_t year = (((cid_bytes[2] & 0xF) << 4) | (cid_bytes[1] >> 4));
    info.date_year = 2000 + year;
    info.date_month = (cid_bytes[1] & 0xF);

    info.valid = true;

    return info;
}
