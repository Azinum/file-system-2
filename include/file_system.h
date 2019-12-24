// file_system.h

#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H

typedef struct FSFILE FSFILE;

int fs_init(unsigned long disk_size);

int fs_init_from_disk(const char* path);

FSFILE* fs_open(const char* path, const char* mode);

FSFILE* fs_create_dir(const char* path);

void fs_close(FSFILE* file);

int fs_write(const void* data, int size, FSFILE* file);

void fs_print_file_info(const FSFILE* file, FILE* output);

void fs_read(const FSFILE* file, FILE* output);

void fs_list(FILE* output);

void fs_dump_disk(const char* path);

void fs_free();

#endif  // _FILE_SYSTEM_H