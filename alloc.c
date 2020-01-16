// alloc.

#include "file_system.h"
#include "block.h"
#include "file.h"
#include "alloc.h"

void flush(unsigned long from, unsigned long to) {
    if (!is_initialized() || from > to || to > get_state()->disk_header->disk_size) {
        return;
    }

    for (unsigned long i = from; i < to; i++) {
        get_state()->disk[i] = 0;
    }
}

void* allocate(unsigned long size) {
    if (!is_initialized()) {
        return NULL;
    }
    unsigned long free_space = 0;
    for (unsigned long i = sizeof(struct FS_disk_header); i < get_state()->disk_header->disk_size; i++) {
        if (free_space >= size) {
            flush(i - free_space, i);
            return (void*)&get_state()->disk[i - free_space];
        }
        switch (get_state()->disk[i]) {
            case BLOCK_USED:
                i += TOTAL_BLOCK_SIZE;
                free_space = 0;
                break;

            case BLOCK_FILE_HEADER:
                i += TOTAL_FILE_HEADER_SIZE;
                free_space = 0;
                break;

            case BLOCK_FREE: {
                free_space += TOTAL_BLOCK_SIZE;
                i += TOTAL_BLOCK_SIZE;
            }
                break;

            case BLOCK_FILE_HEADER_FREE: {
                free_space += TOTAL_FILE_HEADER_SIZE;
                i += TOTAL_FILE_HEADER_SIZE;
            }
                break;

            default:
                free_space++;
                break;
        }
    }
    error("Failed to allocate memory. Disk is full\n");
    return NULL;
}

struct FSFILE* allocate_file(const char* path, int file_type) {
    if (!is_initialized()) {
        return NULL;
    }

    assert((file_type > T_NONE) && (file_type < T_END));

    struct FSFILE* dir = get_ptr(get_state()->disk_header->current_directory);

    unsigned long id = hash2(path);
    unsigned long empty_slot_addr = 0;
    if ((find_file(NULL, id, NULL, &empty_slot_addr))) {
        error(COLOR_MESSAGE "'%s'" NONE " File already exists\n", path);
        return NULL;
    }

    struct FSFILE* file = allocate(TOTAL_FILE_HEADER_SIZE);

    if (file) {
        file->block_type = BLOCK_FILE_HEADER;
        strncpy(file->name, path, FILE_NAME_SIZE);
        file->id = id;
        file->type = file_type;
        file->first_block = 0;

        if (dir) {
            unsigned long file_addr = get_absolute_address(file);
            unsigned long* empty_slot = get_ptr(empty_slot_addr);
            if (empty_slot != NULL)
                *empty_slot = file_addr;
            else
                write_data(&file_addr, sizeof(unsigned long), dir);
        }
    }

    return file;
}

struct Data_block* allocate_blocks(int count) {
    if (!is_initialized() || count <= 0) {
        return NULL;
    }

    struct Data_block* block = allocate(TOTAL_BLOCK_SIZE);
    if (!block) {
        return NULL;
    }

    block->block_type = BLOCK_USED;
    block->bytes_used = 0;
    struct Data_block* next = allocate_blocks(count - 1);
    if (next != NULL) {
        block->next = get_absolute_address(next);
    }
    else {
        block->next = 0;
    }

    return block;
}

int deallocate_file(struct FSFILE* file) {
    if (!is_initialized() || !file) {
        return -1;
    }

    if (!can_access_address(file->first_block)) {
        return 0;
    }

    deallocate_blocks(file->first_block);
    file->first_block = 0;
    file->size = 0;
    return 0;
}

int deallocate_blocks(unsigned long addr) {
    if (!can_access_address(addr)) {
        return -1;
    }
    
    struct Data_block* block = get_ptr(addr);
    unsigned long next = block->next;

    int err = free_block(addr, TOTAL_BLOCK_SIZE, BLOCK_USED);
    if (err != 0) {
        return err;
    }
    
    if (can_access_address(next)) {
        deallocate_blocks(next);
    }
    return 0;
}

int free_block(unsigned long block_addr, unsigned long block_size, char verify_block_type) {
    if (!can_access_address(block_addr)) {
        error("Failed to access address " COLOR_NUMBERS "'%lu'\n" NONE, block_addr);
        return -1;
    }

    char block_type = *(char*)get_ptr(block_addr);
    if (verify_block_type != block_type) {
        error("Failed to free block. Invalid block type (is " COLOR_NUMBERS "%i" NONE ", should be " COLOR_NUMBERS "%i" NONE ")", block_type, verify_block_type);
        return -1;
    }

    flush(block_addr, block_addr + block_size);
    return 0;
}