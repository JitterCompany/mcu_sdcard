#include <string.h>
#include "chip.h"
#include "sdcard.h"
#include "sdcard_reinit.h"

#include <mcu_timing/delay.h>
#include <fatfs_lib/ff.h>
#include <lpc_tools/GPIO_HAL_LPC.h>
#include "file_utils.h"

#include <mcu_timing/profile.h>


// global state //
static mci_card_struct g_sdcardinfo;
static volatile int32_t g_sdio_wait_exit = 0;

static FATFS g_fat_fs;
static bool g_enabled;

static const GPIO *g_sdcard_power_en_pin;
static const GPIO *g_sdcard_detect_pin; // NOTE: unused for now..

// forward declarations //
static void sdmmc_waitms(uint32_t time);
static uint32_t sdmmc_irq_driven_wait(void);
static void sdmmc_setup_wakeup(void *bits);


// NOTE: also used by fs_mci
mci_card_struct* init_cardinfo(void)
{
    memset(&g_sdcardinfo, 0, sizeof(g_sdcardinfo));
    g_sdcardinfo.card_info.evsetup_cb = sdmmc_setup_wakeup;
    g_sdcardinfo.card_info.waitfunc_cb = sdmmc_irq_driven_wait;
    g_sdcardinfo.card_info.msdelay_func = sdmmc_waitms;

    return &g_sdcardinfo;
}

// Delay callback for timed SDIF/SDMMC functions
static void sdmmc_waitms(uint32_t time)
{
    delay_us(time*1000);
    return;
}
/**
 * @brief	A better wait callback for SDMMC driven by the IRQ flag
 * @return	0 on success, or failure condition (-1)
 */
static uint32_t sdmmc_irq_driven_wait(void)
{
    uint32_t status;

    /* Wait for event, would be nice to have a timeout, but keep it  simple */
    while (g_sdio_wait_exit == 0) {}

    /* Get status and clear interrupts */
    status = Chip_SDIF_GetIntStatus(LPC_SDMMC);
    Chip_SDIF_ClrIntStatus(LPC_SDMMC, status);
    Chip_SDIF_SetIntMask(LPC_SDMMC, 0);

    return status;
}

/**
 * @brief	Sets up the SD event driven wakeup
 * @param	bits : Status bits to poll for command completion
 * @return	Nothing
 */
static void sdmmc_setup_wakeup(void *bits)
{
    uint32_t bit_mask = *((uint32_t *)bits);
    /* Wait for IRQ - for an RTOS, you would pend on an event here with a IRQ based wakeup. */
    NVIC_ClearPendingIRQ(SDIO_IRQn);
    g_sdio_wait_exit = 0;
    Chip_SDIF_SetIntMask(LPC_SDMMC, bit_mask);
    NVIC_EnableIRQ(SDIO_IRQn);
}

/**
 * @brief	SDIO controller interrupt handler
 * @return	Nothing
 */
void SDIO_IRQHandler(void)
{
    /* All SD based register handling is done in the callback
       function. The SDIO interrupt is not enabled as part of this
       driver and needs to be enabled/disabled in the callbacks or
       application as needed. This is to allow flexibility with IRQ
       handling for applicaitons and RTOSes. */
    /* Set wait exit flag to tell wait function we are ready. In an RTOS,
       this would trigger wakeup of a thread waiting for the IRQ. */
    NVIC_DisableIRQ(SDIO_IRQn);
    g_sdio_wait_exit = 1;
}

bool sdcard_format()
{
    uint8_t work[FF_MAX_SS];
    FRESULT res = f_mkfs("", FM_FAT32, 0, work, sizeof(work));
    return (res == FR_OK);
}

bool sdcard_create_file(const char *filename)
{
    FIL file;
    if(FR_OK != f_open(&file, filename, FA_CREATE_ALWAYS)) {
        return false;
    }
    return (FR_OK == f_close(&file));
}

bool sdcard_move_file(const char *filename_old, const char *filename_new)
{
    return (FR_OK == f_rename(filename_old, filename_new));
}

bool sdcard_read_file_binary(sdcard_file *fp,
        uint8_t *result, size_t sizeof_result,
        size_t *bytes_returned) {
    
    return (FR_OK == f_read(fp, result, sizeof_result, bytes_returned));
}

bool sdcard_write_to_file_offset(
    const char *filename,
    const char *data,
    const size_t size,
    const size_t offset)
{
    FIL file;
    if(FR_OK != f_open(&file, filename, FA_WRITE | FA_OPEN_ALWAYS))
    {
        return false;
    }
    
    bool success = false; 
    if(FR_OK == f_lseek(&file, offset)) {

        UINT bw;
        success = (FR_OK == f_write(&file, data, size, &bw));
    }
    
    return (FR_OK == f_close(&file)) && success;
}

bool sdcard_write_to_file(
    const char *filename,
    const char *data,
    const uint32_t size)
{
    FIL file;
    if(FR_OK != f_open(&file, filename, FA_WRITE | FA_OPEN_ALWAYS)) {
        return false;
    }

    bool success = false;

    if(FR_OK == f_lseek(&file, f_size(&file))) {

        UINT bw;
        success = (FR_OK == f_write(&file, data, size, &bw));
    }

    return (FR_OK == f_close(&file)) && success;
}

bool sdcard_delete_file(const char *filename)
{
    return (FR_OK == f_unlink(filename));
}

bool sdcard_detect(void)
{
#if SD_CD_pin == 1
    return Chip_SDIF_CardNDetect(LPC_SDMMC);
#else
    //TODO: implement
    // read SD3 line to determine sdcard presence
    return true;
#endif
}

bool sdcard_free_space(uint32_t *free_MiB, uint32_t *total_MiB)
{
    uint32_t fre_clust = 0;
    // Get volume information and free clusters of drive 0
    FATFS *fs = &g_fat_fs;
    if(FR_OK != f_getfree("", &fre_clust, &fs)) {
        return false;
    }

    // Get total sectors and free sectors, calculate in MB,
    // assuming 512 bytes/sector
    uint32_t tot_sect = ((fs->n_fatent - 2) * fs->csize);
    *total_MiB = tot_sect >> 11;
    *free_MiB = (fre_clust * fs->csize) >> 11;
    
    return true;
}

bool sdcard_create_dir(const char *dir)
{
    return (FR_OK == f_mkdir(dir));
}

bool sdcard_read_lines(const char *filename, char *line_buf,
                       const uint32_t n, line_handler cb, void *cb_ctx)
{
    FIL file;
    if(FR_OK != f_open(&file, filename,  FA_OPEN_EXISTING | FA_READ)) {
        return false;
    }

    for (;;) {
        char *rbuf = f_gets(line_buf, n, &file);

        if (!rbuf) {
            break;
        }
        if(!cb(cb_ctx, rbuf)) {
            break;
        }
    }

    return (FR_OK == f_close(&file));
}

bool sdcard_file_exists(const char *filename)
{
    sdcard_file fp;
    bool exists = sdcard_open_file(&fp, filename);

    if(exists) {
        sdcard_close_file(&fp);
    }
    return exists;    
}

size_t sdcard_file_size(const char *filename)
{
    sdcard_file fp;
    if(sdcard_open_file(&fp, filename)) {
        const size_t result = f_size(&fp);
        sdcard_close_file(&fp);
        return result;
    }
    return 0;
}

bool sdcard_open_file(sdcard_file *fp, const char *filename)
{
    FRESULT rc = f_open(fp, filename,  FA_OPEN_EXISTING | FA_READ);
    return !rc;
}

bool sdcard_close_file(sdcard_file *fp)
{
    FRESULT rc = f_close(fp);
    return !rc;
}

char *sdcard_read_file(char *str, int n, sdcard_file *fp)
{
    return f_gets(str, n, fp);
}

bool sdcard_read_file_offset(sdcard_file *fp, size_t offset,
        uint8_t *result, size_t sizeof_result,
        size_t *bytes_returned) {

    FRESULT rc = f_lseek(fp, offset);
    if (rc != FR_OK) {
        return false;
    }

     rc = f_read (fp, result, sizeof_result, bytes_returned);
    return !rc;
}


bool sdcard_is_enabled(void) 
{
    return g_enabled;
}


enum SDCardStatus sdcard_reset(void)
{
    // SD off
    delay_us(100*1000);
    sdcard_disable();

    // SD on again
    delay_us(100*1000);
    return sdcard_enable(NULL);
}


enum SDCardStatus sdcard_enable(int *retry_count)
{
    // some delay seems to be required if sdcard_disable() was called before
    delay_us(10*1000);

    // NOTE: it is important to do this first
    Chip_SDIF_Init(LPC_SDMMC);

    // enable power
    if (g_sdcard_power_en_pin) {
        Chip_GPIO_SetPinState(LPC_GPIO_PORT,g_sdcard_power_en_pin->port,
                          g_sdcard_power_en_pin->pin, true);
    }

    init_cardinfo();

    NVIC_EnableIRQ(SDIO_IRQn);

    FRESULT rc;

    const int MAX_ENABLE_RETRIES = 5; 
    int tries;
    for(tries=0;tries<MAX_ENABLE_RETRIES;tries++) {

        // Wait for SDCard to boot. Note: switch rise time is 2.5 ms
        delay_us(4000);

        // mount filesystem
        rc = f_mount(&g_fat_fs, "", 1);

        if(FR_OK == rc) {
            g_enabled = true;
            if(tries && (retry_count != NULL)) {
                *retry_count = tries;
            }
            return SDCARD_OK;
        }
    }
    
    if (FR_NOT_READY == rc) {
        return SDCARD_NOT_FOUND;
    }
    return SDCARD_ERROR;
}


void sdcard_disable(void)
{
    //try to unmount: we ignore the return code, because nothing can
    //be done if it fails
    f_mount(NULL, "", 0);

    disk_uninitialize(0);

    // disable power
    if (g_sdcard_power_en_pin) {
        Chip_GPIO_SetPinState(LPC_GPIO_PORT,g_sdcard_power_en_pin->port,
                          g_sdcard_power_en_pin->pin, false);
    }

    NVIC_DisableIRQ(SDIO_IRQn);
    uint32_t status = Chip_SDIF_GetIntStatus(LPC_SDMMC);
    Chip_SDIF_ClrIntStatus(LPC_SDMMC, status);
    Chip_SDIF_SetIntMask(LPC_SDMMC, 0);

    g_enabled = false;
    // DO NOT call Chip_SDIF_DeInit(LPC_SDMMC), this somehow freezes the chip
    // Chip_SDIF_DeInit(LPC_SDMMC);

    // NOTE: this is required somehow.
    // When leaving this out, the next sdcard_enable() will fail
    Chip_SDIF_Init(LPC_SDMMC);
}


void sdcard_deinit(void)
{
    NVIC_DisableIRQ(SDIO_IRQn);
    uint32_t status = Chip_SDIF_GetIntStatus(LPC_SDMMC);
    Chip_SDIF_ClrIntStatus(LPC_SDMMC, status);
    Chip_SDIF_SetIntMask(LPC_SDMMC, 0);

    Chip_SDIF_DeInit(LPC_SDMMC);
}

void sdcard_init(const GPIO *power_en_pin, const GPIO *sd_detect_pin)
{
    g_sdcard_power_en_pin = power_en_pin;
    g_sdcard_detect_pin = sd_detect_pin;
}


int sdcard_scan_dir(char *result_list, size_t sizeof_result_list,
        const char *dir_str)
{
    if(result_list && (sizeof_result_list < 1)) {
        return -1;
    }
    char *filename = result_list;

    // reserve one extra byte for double null terminator at the end
    size_t filename_size = sizeof_result_list - 1;

    DIR dir;
    if (FR_OK != f_opendir(&dir, dir_str)) {
        return -2;
    }

    int result = 0;
    for (;;) {
        FILINFO fno;
        if (FR_OK != f_readdir(&dir, &fno))
        {
            result = -3;
            break;
        }
        if(fno.fname[0] == 0) {
            break;  // Break on end of dir
        }

        if(filename != NULL) {
            size_t len = strlcpy(filename, fno.fname, filename_size);
            if(len >= filename_size) {
                result = -1;
                break;
            }
            filename+=(len+1);
            filename_size-=(len+1);
        }
        ++result;
    }
    if(filename != NULL) {
        *filename = '\0'; // extra null terminator: end of array
    }
    f_closedir(&dir);

    return result;
}

int sdcard_iterate_dir(const char *dirname, SDCardIterateCB cb, void *cb_ctx)
{
    int result_count = 0;

    DIR dir;
    const FRESULT open_res = f_opendir(&dir, dirname);
    if (FR_OK != open_res) {
        return -2;
    }

    char path[strlen(dirname) + 1 + FILENAME_MAX_LEN + 1];
    for (;;) {
        FILINFO fno;
        if (FR_OK != f_readdir(&dir, &fno))
        {
            result_count = -3;
            break;
        }
        if(fno.fname[0] == 0) {
            break;  // Break on end of dir
        }

        if(make_file_path(path, sizeof(path), dirname, fno.fname)) {
            result_count+= 1;
            cb(cb_ctx, path, fno.fname);
        } else {
            result_count = -1;
            break;
        }
    }
    f_closedir(&dir);

    return result_count;
}

static void delete_cb(void *dummy_null, const char *path, const char *fname)
{
    sdcard_delete_file(path);
}

bool sdcard_delete_dir(const char *dirname)
{
    int delete_count = sdcard_iterate_dir(dirname, delete_cb, NULL);

    return ((FR_OK == f_unlink(dirname)) || (delete_count == -2));
}

