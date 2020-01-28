// file_system.c

#include "file_system.h"
#include "file.h"
#include "block.h"
#include "alloc.h"
#include "dir.h"

static struct FS_state fs_state;

int is_initialized() {
    return get_state()->is_initialized;
}

void fslog(char* format, ...) {
    if (!is_initialized())
        return;
    
    time_t current_time = time(NULL);
    char* time_str = ctime(&current_time);
    time_str[strlen(time_str) - 1] = '\0';
    va_list args;
    va_start(args, format);

    if (get_state()->log) {
        fprintf(get_state()->log, "(%s) ", time_str);
        vfprintf(get_state()->log, format, args);
    }
    else {
        vfprintf(stdout, format, args);
    }
    
    va_end(args);
}

struct FS_state* get_state() {
	return &fs_state;
}

struct FSFILE* find_file(struct FSFILE* dir, unsigned long id, unsigned long* location, unsigned long* empty_slot) {
    if (!is_initialized()) {
        return NULL;
    }

    if (!dir)
        dir = get_ptr(get_state()->disk_header->current_directory);

    if (!dir) {
        fslog("[Warning] Directory is undefined\n");
        return NULL;
    }

    struct Data_block* block = NULL;

    addr_t next = dir->first_block;
    if (next == 0)
        return NULL;

    int skip = 2;   // Skip the two first files (current and parent directory)
    struct FSFILE* file = NULL;
    while ((block = read_block(next)) != NULL) {
        addr_t* data = (addr_t*)block->data;
        for (int i = 0; i < block->bytes_used / sizeof(addr_t); i++) {
            if (skip) {
                --skip;
                continue;
            }
            addr_t addr = data[i];
            if (addr == 0) {
                if (empty_slot) *empty_slot = get_absolute_address(&data[i]);
                continue;
            }
            if ((file = get_ptr(addr))) {
                if (file->id == id) {
                    if (location != NULL) {
                        *location = get_absolute_address(&data[i]);
                    }
                    return file;
                }
            }
        }

        if (block->next == 0) {
            return NULL;
        }
        next = block->next;
    }

    return NULL;
}

int write_data(const void* data, unsigned long size, struct FSFILE* file) {
    if (!file || !is_initialized()) {
        return -1;
    }

    if ((MODE_WRITE != (file->mode & MODE_WRITE) && MODE_APPEND != (file->mode & MODE_APPEND)) && file->type != T_DIR) {
        error(COLOR_MESSAGE "'%s'" NONE ": Failed to write data, file mode (write/append) isn't set\n");
        return -1;
    }

    unsigned long bytes_written = 0;
    
    if (file->first_block != 0) {
        struct Data_block* first = get_ptr(file->first_block);
        if (first) {
            struct Data_block* last = get_last_block(first);
            if (last) {
                unsigned long last_addr = get_absolute_address(last);
                if (can_access_address(last_addr)) {
                    if ((BLOCK_SIZE - last->bytes_used) >= size) {
                        write_to_blocks(file, data, size, &bytes_written, last_addr);
                    }
                    else {
                        int block_count = (size + last->bytes_used) / BLOCK_SIZE + 1;
                        struct Data_block* block = allocate_blocks(block_count);
                        if (block) {
                            unsigned long addr = get_absolute_address(block);
                            if (can_access_address(addr)) {
                                unsigned long bytes_written = 0;
                                write_to_blocks(file, data, size, &bytes_written, addr);
                                last->next = addr;
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        int block_count = size / BLOCK_SIZE + 1;
        struct Data_block* block = allocate_blocks(block_count);
        if (block) {
            unsigned long bytes_written = 0;
            unsigned long addr = get_absolute_address(block);
            write_to_blocks(file, data, size, &bytes_written, addr);
            file->first_block = addr;
        }
    }

    if (bytes_written != size) {
        return -1;
    }
    return 0;
}

// Different cases:
// - When bytes_used != 0
// - When the size is less than BLOCK_SIZE - bytes_used
// - When the size is greater than BLOCK_SIZE - bytes_used but is less than BLOCK_SIZE
void write_to_blocks(struct FSFILE* file, const void* data, unsigned long size, unsigned long* bytes_written, unsigned long block_addr) {
    if (size == 0)
        return;

    if (!can_access_address(block_addr))
        return;

    struct Data_block* block = get_ptr(block_addr);

    if (!block)
        return;

    unsigned long bytes_avaliable = (BLOCK_SIZE - (block->bytes_used));

    unsigned long bytes_to_write = bytes_avaliable;

    if (size <= bytes_avaliable) {
        bytes_to_write = size;
    }

    // If this block is already filled, go to next one
    if ((bytes_avaliable == 0) && can_access_address(block->next)) {
        write_to_blocks(file, data, size, bytes_written, block->next);
        return;
    }

    fslog("Writing %lu bytes to file '%s'\n", bytes_to_write, file->name);

    memcpy(block->data + block->bytes_used, data, bytes_to_write);
    *bytes_written += bytes_to_write;
    block->bytes_used += bytes_to_write;
    file->size += bytes_to_write;

    if (can_access_address(block->next)) {
        if (size - bytes_to_write > 0) {
            write_to_blocks(file, data + bytes_to_write, size - bytes_to_write, bytes_written, block->next);
        }
    }
}

void* get_ptr(unsigned long address) {
    if (!is_initialized() || !get_state()->disk) {
        return NULL;
    }
    if (can_access_address(address)) {
        return (void*)get_state()->disk + address;
    }
    return NULL;
}

// Addresses == 0 are invalid
unsigned long get_absolute_address(const void* address) {
    if (!is_initialized() || !get_state()->disk) {
        return 0;
    }
    return (unsigned long)address - (unsigned long)get_state()->disk;
}

int can_access_address(unsigned long address) {
    if (!is_initialized())
        return 0;
    if (address == 0 || address >= get_state()->disk_header->disk_size)
        return 0;

    return 1;
}