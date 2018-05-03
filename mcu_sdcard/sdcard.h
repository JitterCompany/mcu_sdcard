#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <lpc_tools/GPIO_HAL.h>
#include <fatfs_lib/ff.h>

//set to 1 if SD_CD pin is connected
#define SD_CD_pin 1

typedef bool (*line_handler)(void *context, char *line);
typedef void (*DiskIOCallback)(void);

typedef FIL sdcard_file;

void sdcard_init(const GPIO *power_en_pin,
        const GPIO *sd_detect_pin,
        DiskIOCallback disk_IO_callback);
void sdcard_deinit(void);
bool sdcard_format(void);
bool sdcard_detect(void);
bool sdcard_file_exists(const char *filename);
size_t sdcard_file_size(const char *filename);
bool sdcard_write_to_file(const char *filename,
                          const char *data, const uint32_t size);
bool sdcard_write_to_file_offset(const char *filename,
                          const char *data, const size_t size,
                          const size_t offset);
bool sdcard_create_file(const char *filename);
bool sdcard_move_file(const char *filename_old, const char *filename_new);
bool sdcard_delete_file(const char *filename);

/**
 * Delete a folder and any files in it.
 *
 * NOTE: the delete will fail if the folder contains sub-folders.
 */
bool sdcard_delete_dir(const char *dirname);
bool sdcard_create_dir(const char *dir);
bool sdcard_free_space(uint32_t *free_MiB, uint32_t *total_MiB);

/**
 * scard_scan_dir: count the amount of files in a dir.
 * If result_list is non-NULL, the resulting list of files in folder
 * is returned to it.
 * 
 * -1: buffer too small
 * -2: error opening folder
 * -3: error reading directory item
 *  n: n files found. If result_list is not NULL, files are returned
 *     as a string in <file1>\0<file2>\0<filen>\0\0 format
 */
int sdcard_scan_dir(char *result_list, size_t sizeof_result_list,
        const char *dir_str);

/**
 * scard_iterate_dir: callback for each item in a dir.
 * 
 * -1: error creating file path
 * -2: error opening folder
 * -3: error reading directory item
 *  n: n files found. The callback was called n times.
 */
typedef void (SDCardIterateCB)(void *cb_ctx,
        const char *path, const char *fname);
int sdcard_iterate_dir(const char *dirname, SDCardIterateCB cb, void *cb_ctx);

bool sdcard_read_lines(const char *filename, char *line_buf,
                       const uint32_t n, line_handler cb, void *cb_ctx);

enum SDCardStatus {
    SDCARD_OK = 0,
    SDCARD_NOT_FOUND = -1,
    SDCARD_ERROR = -2
};

bool sdcard_is_enabled(void);
enum SDCardStatus sdcard_enable(int *retry_count);
void sdcard_disable(void);

enum SDCardStatus sdcard_reset(void);

bool sdcard_open_file(sdcard_file *fp, const char *filename);
bool sdcard_close_file(sdcard_file *fp);
char *sdcard_read_file(char *str, int n, sdcard_file *fp);
bool sdcard_read_file_binary(sdcard_file *fp,
        uint8_t *result, size_t sizeof_result,
        size_t *bytes_returned);
bool sdcard_read_file_offset(sdcard_file *fp, size_t offset,
        uint8_t *result, size_t sizeof_result,
        size_t *bytes_returned);


#endif
