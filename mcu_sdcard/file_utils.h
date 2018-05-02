#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define FILENAME_MAX_LEN         (8 + 1 + 3)
#define PATHNAME_MAX_LEN         ((FILENAME_MAX_LEN *2) + 4)
#define FILESIZE_MAX             (1024*1024*1024)

int folder_create_and_count(const char *name);
bool make_file_path(char *result_path, size_t sizeof_result_path,
                    const char *folder, const char *file);
bool move_file(const char *dst_folder, const char *src_folder,
               const char *filename);
#endif

