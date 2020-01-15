// file_system.h

#ifndef _FILE_SYSTEM
#define _FILE_SYSTEM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>

#include "config.h"
#include "hash.h"

#define HEADER_MAGIC 0xbeefaaaa

struct FS_disk_header {
    int magic;
    unsigned long disk_size;
    unsigned long root_directory;
    unsigned long current_directory;
};

struct FS_state {
    char* disk;
    int is_initialized;
    int error;
    FILE* err;
    FILE* log;
    struct FS_disk_header* disk_header;
};

typedef struct FSFILE FSFILE;

int is_initialized();

void error(char* format, ...);

void fslog(char* format, ...);

struct FS_state* get_state();

FSFILE* find_file(FSFILE* dir, unsigned long id, unsigned long* position, unsigned long* empty_slot);

int write_data(const void* data, unsigned long size, FSFILE* file);

void write_to_blocks(FSFILE* file, const void* data, unsigned long size, unsigned long* bytes_written, unsigned long block_addr);

// Get pointer from address/index on disk
void* get_ptr(unsigned long address);

unsigned long get_absolute_address(void* address);

int can_access_address(unsigned long address);

#endif