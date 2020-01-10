// block.h

#ifndef _BLOCK_H
#define _BLOCK_H

#define BLOCK_SIZE 32

enum Block_types {
    BLOCK_USED = 1,
    BLOCK_FREE,
    BLOCK_FILE_HEADER,
    BLOCK_FILE_HEADER_FREE
};

struct Data_block {
    char block_type;    // BLOCK_USED, BLOCK_FREE
    char data[BLOCK_SIZE];
    int bytes_used;     // Number of bytes written in this block
    unsigned long next;
};

#define TOTAL_BLOCK_SIZE sizeof(struct Data_block)

void print_block_info(struct Data_block* block, FILE* output);

#endif // _BLOCK_H