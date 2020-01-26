// dir.h

#ifndef _DIR_H
#define _DIR_H

#include <stdio.h>

int find_in_dir(const struct FSFILE* dir, const struct FSFILE* file, addr_t** location);

int read_dir_contents(const struct FSFILE* file, unsigned long block_addr, int iteration, FILE* output);

struct FSFILE* get_path_dir(const char* path, struct FSFILE** file);

int print_working_directory(FILE* output);

int is_dir(const struct FSFILE* file);

int can_remove_dir(const struct FSFILE* file);

int fill_empty_file_slots(struct Data_block* block, int from_index);

#endif