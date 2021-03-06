// fs2.h

#ifndef _FS2_H
#define _FS2_H

#include "config.h"

#define ARRAY_SIZE(arr) ((sizeof(arr)) / (sizeof(arr[0])))

typedef struct FSFILE FSFILE;

int fs_init(unsigned long disk_size);

int fs_init_from_disk(const char* path);

FSFILE* fs_open(const char* path, const char* mode);

FSFILE* fs_open_dir(const char* path);

FSFILE* fs_create_dir(const char* path);

int fs_change_dir(const char* path);

int fs_remove_file(const char* path);

void fs_close(FSFILE* file);

int fs_write(const void* data, unsigned long size, FSFILE* file);

void fs_print_file_info(const FSFILE* file, FILE* output);

int fs_pwd(FILE* output);

int fs_read(const FSFILE* file, FILE* output);

int fs_list(const char* path, FILE* output);

void fs_dump_disk(const char* path);

int fs_get_error();

void fs_free();

#endif  // _FS2_H