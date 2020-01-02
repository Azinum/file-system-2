// file.h

#ifndef _FILE_H
#define _FILE_H

#define FILE_NAME_SIZE 32
#define BLOCK_SIZE 32

enum File_types {
    T_FILE = 1,
    T_DIR
};

enum Block_types {
    BLOCK_USED = 1,
    BLOCK_FREE,
    BLOCK_FILE_HEADER,
    BLOCK_FILE_HEADER_FREE
};

enum File_mode {
    MODE_NONE   = 0 << 0,
    MODE_READ   = 1 << 0,
    MODE_WRITE  = 1 << 1,
    MODE_APPEND = 1 << 2
};

struct Data_block {
    int block_type;    // BLOCK_USED, BLOCK_FREE
    char data[BLOCK_SIZE];
    int bytes_used;     // Number of bytes written in this block
    unsigned long next;
};

struct FSFILE {
    int block_type;    // BLOCK_FILE_HEADER, BLOCK_FILE_HEADER_FREE
    char name[FILE_NAME_SIZE];
    unsigned long hashed_name;
    int size;   // Size in bytes
    // int links_count;
    int type;   // T_FILE, T_DIR
    int mode;   // MODE_NONE, MODE_READ, MODE_WRITE, MODE_APPEND
    unsigned long first_block;
};

#define TOTAL_BLOCK_SIZE sizeof(struct Data_block)
#define TOTAL_FILE_HEADER_SIZE sizeof(struct FSFILE)

#endif // _FILE_H