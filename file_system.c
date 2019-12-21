// file_system.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "file_system.h"
#include "file.h"
#include "hash.h"

#define HEADER_MAGIC 0xbeefaaaa

struct FS_disk_header {
    int magic;
    unsigned int disk_size;
    unsigned long root_directory;
};

struct FS_state {
    char* disk;
    int is_initialized;
    struct FS_disk_header disk_header;
    FSFILE* current_directory;
};

static struct FS_state fs_state;

static int is_initialized();
static struct Data_block* allocate_blocks(int size);
static void flush(int from, int to);
static void* allocate(int size);
static FSFILE* allocate_file(const char* path, int file_type);
static void write_to_blocks(const void* data, int size, int* bytes_written, struct Data_block* block);
static void read_file_contents(struct Data_block* block, FILE* output);

inline unsigned long get_absolute_address(void* address);

int is_initialized() {
    return fs_state.is_initialized;
}

static void* allocate(int size) {
    if (!is_initialized()) {
        return NULL;
    }
    void* block = NULL;
    int free_space = 0;
    for (int i = sizeof(struct FS_disk_header); i < fs_state.disk_header.disk_size; i++) {
        if (free_space >= size) {
            return (void*)&fs_state.disk[i - (size + (free_space % size))];
        }
        switch (fs_state.disk[i]) {
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
                flush(i, i + TOTAL_BLOCK_SIZE);
            }
                break;

            case BLOCK_FILE_HEADER_FREE: {
                free_space += TOTAL_FILE_HEADER_SIZE;
                flush(i, i + TOTAL_FILE_HEADER_SIZE);
            }
                break;

            default:
                free_space++;
                break;
        }
    }

    return NULL;
}

struct Data_block* allocate_blocks(int size) {
    if (!is_initialized()) {
        return NULL;
    }
    if (size <= 0) {
        return NULL;
    }

    struct Data_block* block = allocate(TOTAL_BLOCK_SIZE);

    if (block != NULL) {
        block->block_type = BLOCK_USED;
        block->bytes_used = 0;
        struct Data_block* next = allocate_blocks(size - BLOCK_SIZE);
        if (next != NULL) {
            block->next = get_absolute_address(next);
        }
        else {
            block->next = 0;
        }
    }

    return block;
}

void flush(int from, int to) {
    if (!is_initialized() || from > to) {
        return;
    }
    if (to > fs_state.disk_header.disk_size) {
        return;
    }

    for (int i = from; i < to; i++) {
        fs_state.disk[i] = 0;
    }

}

FSFILE* allocate_file(const char* path, int file_type) {
    if (!is_initialized()) {
        return NULL;
    }

    FSFILE* dir = fs_state.current_directory;
    (void)dir;

    FSFILE* file;

    file = allocate(TOTAL_FILE_HEADER_SIZE);
    if (file) {
        file->block_type = BLOCK_FILE_HEADER;
        strcpy(file->name, path);
        file->type = file_type;
    }

    return file;
}

// Different cases:
// - When bytes_used != 0
// - When the size is less than BLOCK_SIZE - bytes_used
// - When the size is greater than BLOCK_SIZE - bytes_used but is less than BLOCK_SIZE
void write_to_blocks(const void* data, int size, int* bytes_written, struct Data_block* block) {
    if (size <= 0) {
        return;
    }
    if (!block) {
        return;
    }
    
    int bytes_avaliable = (BLOCK_SIZE - (block->bytes_used));

    int bytes_to_write = 0;

    if (size <= bytes_avaliable) {
        bytes_to_write = size;
    }
    else {
        bytes_to_write = size - bytes_avaliable;
    }

    if (block->bytes_used >= BLOCK_SIZE || bytes_avaliable == 0) {
        struct Data_block* next = (struct Data_block*)&fs_state.disk[block->next];
        if (next) {
            write_to_blocks(data, size, bytes_written, next);
        }
        return;
    }

    memcpy(block->data + block->bytes_used, data, bytes_to_write);
    *bytes_written += bytes_to_write;
    block->bytes_used += bytes_to_write;

    if (block->next != 0) {
        struct Data_block* next = (struct Data_block*)&fs_state.disk[block->next];
        if (next) {
            write_to_blocks(data + bytes_to_write, size - bytes_to_write, bytes_written, next);
        }
    }
}

void read_file_contents(struct Data_block* block, FILE* output) {
    if (!block || !output || !is_initialized()) {
        return;
    }
    fprintf(output, "%s", block->data);

    if (block->next <= 0) {
        return;
    }
    struct Data_block* next = (struct Data_block*)&fs_state.disk[block->next];
    if (next) {
        read_file_contents(next, output);
    }
}

// Addresses <= 0 are invalid
unsigned long get_absolute_address(void* address) {
    if (!is_initialized()) {
        return 0;
    }
    if (!fs_state.disk) {
        return 0;
    }
    return (unsigned long)address - (unsigned long)fs_state.disk;
}

int fs_init(int disk_size) {
    if (is_initialized()) {
        return -1;
    }
    fs_state.is_initialized = 1;
    fs_state.disk_header.disk_size = sizeof(char) * disk_size;
    fs_state.disk = malloc(fs_state.disk_header.disk_size);
    if (!fs_state.disk) {
        fprintf(stderr, "%s: Failed to allocate memory for disk\n", __FUNCTION__);
        return -1;
    }
    struct FS_disk_header* header = allocate(sizeof(struct FS_disk_header));
    if (!header) {
        fprintf(stderr, "Failed to allocate memory for disk header\n");
        return -1;
    }

    header->magic = HEADER_MAGIC;
    FSFILE* root = fs_create_dir("");
    if (!root) {
        fprintf(stderr, "Failed to create root directory\n");
        return -1;
    }
    fs_state.current_directory = root;
    header->root_directory = (unsigned long)root - (unsigned long)fs_state.disk;
    return 0;
}

FSFILE* fs_open(const char* path, const char* mode) {
    if (!is_initialized()) {
        return NULL;
    }
    if (*mode != 'r' && *mode != 'w' && *mode != 'a') {
        return NULL;
    }
    FSFILE* file;

    switch (*mode) {
        case 'w': {
            file = allocate_file(path, T_FILE);
            if (file) {
                file->mode = MODE_WRITE;
                return file;
            }
        }
            break;

        case 'r': {
            printf("File mode READ\n");

        }
            break;

        case 'a': {
            printf("File mode APPEND\n");

        }
            break;

        default:
            break;
    }

    return NULL;
}

FSFILE* fs_create_dir(const char* path) {
    if (!is_initialized()) {
        return NULL;
    }

    FSFILE* file = allocate_file(path, T_DIR);
    if (file) {

        return file;
    }
    return NULL;
}

void fs_close(FSFILE* file) {
    if (!file) {
        return;
    }
    if (file->mode != 0) {
        file->mode = 0;
    }
}

// TODO: Cleanup!
int fs_write(const void* data, int size, FSFILE* file) {
    if (!file || !is_initialized()) {
        return -1;
    }
    if (MODE_WRITE != (file->mode & MODE_WRITE)) {
        return -1;
    }
    int bytes_written = 0;

    if (file->first_block != 0) {

        struct Data_block* block = (struct Data_block*)&fs_state.disk[file->first_block];
        if (block) {
            if (size < (BLOCK_SIZE - block->bytes_used)) {
                write_to_blocks(data, size, &bytes_written, block);
            }
            else {
                struct Data_block* next_blocks = allocate_blocks(size);
                if (next_blocks) {
                    unsigned long addr = get_absolute_address(next_blocks);
                    block->next = addr;
                    write_to_blocks(data, size, &bytes_written, block);
                }
            }
        }
    }
    else {
        struct Data_block* block = allocate_blocks(size);
        if (block) {
            file->first_block = get_absolute_address(block);
            write_to_blocks(data, size, &bytes_written, block);
        }
    }

    if (bytes_written != size) {
        return -1;
    }
    return 0;
}

void fs_print_file_info(const FSFILE* file, FILE* output) {
    if (!file || !output) {
        return;
    }

    fprintf(output, "name: %s", file->name);
    if (file->type == T_DIR) fprintf(output, "/");
    fprintf(output, "\n");
    fprintf(output, "type: %i\n", file->type);
    /* fprintf(output, "size: %i bytes\n", get_file_size()); */
    fprintf(output, "mode: %i\n", file->mode);
    fprintf(output, "first block: $%lu\n", file->first_block);
}

void fs_read(const FSFILE* file, FILE* output) {
    if (!file || !output || !is_initialized()) {
        return;
    }
    if (file->first_block == 0) {
        return;
    }
    struct Data_block* first_block = (struct Data_block*)&fs_state.disk[file->first_block];

    if (first_block) {
        read_file_contents(first_block, output);
    }
}

void fs_dump_disk(const char* path) {
    if (!is_initialized()) {
        return;
    }
    FILE* file = fopen(path, "w");
    if (file) {
        fwrite(fs_state.disk, sizeof(char), fs_state.disk_header.disk_size, file);
        fclose(file);
    }
}

void fs_free() {
    if (fs_state.is_initialized) {
        if (fs_state.disk) {
            free(fs_state.disk);
        }
    }
}