// alloc.h

#ifndef _ALLOC_H
#define _ALLOC_H

void flush(unsigned long from, unsigned long to);

void* allocate(unsigned long size);

struct FSFILE* allocate_file(const char* path, int file_type);

struct Data_block* allocate_blocks(int count);

int deallocate_file(struct FSFILE* file);

int deallocate_blocks(unsigned long addr);

int free_block(unsigned long block_addr, unsigned long block_size, char verify_block_type);

#endif