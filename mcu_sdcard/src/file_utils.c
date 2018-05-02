#include "file_utils.h"
#include "sdcard.h"
#include <c_utils/max.h>
#include <c_utils/assert.h>
#include <string.h>
#include <stdio.h>

int folder_create_and_count(const char *name)
{
    int file_count = sdcard_scan_dir(NULL, 0, name);
    if(file_count < 0) {
        sdcard_create_dir(name);
    }
    return max(0, file_count);
}

bool make_file_path(char *result_path, size_t sizeof_result_path,
                    const char *folder, const char *file)
{
    result_path[0] = '\0';
    strlcpy(result_path, folder, sizeof_result_path);
    strlcat(result_path, "/", sizeof_result_path);
    size_t len = strlcat(result_path, file, sizeof_result_path);
    if(len >= sizeof_result_path) {
        return false;
    }
    return true;
}

bool move_file(const char *dst_folder, const char *src_folder,
               const char *filename)
{
    char dst_path[PATHNAME_MAX_LEN+1];
    char src_path[PATHNAME_MAX_LEN+1];
    assert(make_file_path(dst_path, sizeof(dst_path), dst_folder, filename));
    assert(make_file_path(src_path, sizeof(src_path), src_folder, filename));

    if(!sdcard_file_exists(src_path)) {
        return false;
    }

    if(sdcard_file_exists(dst_path)) {
        sdcard_delete_file(dst_path);
    }
    if(!sdcard_move_file(src_path, dst_path)) {
        return false;
    }
    return true;
}

