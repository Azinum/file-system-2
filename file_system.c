// file_system.c
// tab size: 4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>
#include <dirent.h>

#include "file_system.h"
#include "file.h"
#include "hash.h"
#include "read.h"
#include "block.h"

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

static struct FS_state fs_state;

static int is_initialized();
static void error(char* format, ...);
static void fslog(char* format, ...);

static int initialize(struct FS_state* state, unsigned long disk_size);


// TODO(lucas): Move some of these functions to other suitable files
// Suggestion: move all block (alloc, dealloc) related logic to block.c
static struct Data_block* allocate_blocks(int count);
static void flush(unsigned long from, unsigned long to);
static void* allocate(unsigned long size);
static FSFILE* allocate_file(const char* path, int file_type);
static int deallocate_file(FSFILE* file);
static int deallocate_blocks(unsigned long addr);
static int remove_file(const char* path, int file_type);
static int free_block(unsigned long block_addr, unsigned long block_size, char verify_block_type);
static FSFILE* find_file(unsigned long id, unsigned long* position, unsigned long* empty_slot, int file_type);
static struct Data_block* read_block(unsigned long block_addr);
static struct Data_block* get_last_block(struct Data_block* block);
static void write_to_blocks(FSFILE* file, const void* data, unsigned long size, unsigned long* bytes_written, unsigned long block_addr);
static void read_file_contents(unsigned long block_addr, FILE* output);
static int read_dir_contents(const FSFILE* file, unsigned long block_addr, FILE* output);
static int count_blocks(struct Data_block* block);
static int get_size_of_blocks(unsigned long block_addr);

// Get pointer from address/index on disk
inline void* get_ptr(unsigned long address);
inline unsigned long get_absolute_address(void* address);
inline int can_access_address(unsigned long address);

int is_initialized() {
    return fs_state.is_initialized;
}

void error(char* format, ...) {
    int init = is_initialized();
    if (init) fs_state.error = 1;

    va_list args;
    va_start(args, format);
    
    if (fs_state.err && init)
        vfprintf(fs_state.err, format, args);
    else
        vfprintf(stderr, format, args);
    
    va_end(args);
}

void fslog(char* format, ...) {
    if (!is_initialized())
        return;
    
    time_t current_time = time(NULL);
    char* time_str = ctime(&current_time);
    time_str[strlen(time_str) - 1] = '\0';
    va_list args;
    va_start(args, format);

    if (fs_state.log) {
        fprintf(fs_state.log, "(%s) ", time_str);
        vfprintf(fs_state.log, format, args);
    }
    else {
        vfprintf(stdout, format, args);
    }
    
    va_end(args);
}

static void* allocate(unsigned long size) {
    assert(size != ULONG_MAX);
    if (!is_initialized()) {
        return NULL;
    }
    unsigned long free_space = 0;
    for (unsigned long i = sizeof(struct FS_disk_header); i < fs_state.disk_header->disk_size; i++) {
        if (free_space >= size) {
            flush(i - free_space, i);
            return (void*)&fs_state.disk[i - free_space];
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

    return NULL;
}

int initialize(struct FS_state* state, unsigned long disk_size) {
    if (!state) {
        return -1;
    }
    state->is_initialized = 1;
    fs_state.error = 0;
    state->err = NULL;
    state->log = NULL; //fopen("log/disk_events.log", "ab");

    state->disk_header = (struct FS_disk_header*)state->disk;
    state->disk_header->magic = HEADER_MAGIC;
    state->disk_header->disk_size = sizeof(char) * disk_size;
    FSFILE* root = fs_create_dir("root");
    if (!root) {
        error("Failed to create root directory\n");
        return -1;
    }
    state->disk_header->current_directory = state->disk_header->root_directory = get_absolute_address(root);
    return 0;
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

void flush(unsigned long from, unsigned long to) {
    if (!is_initialized() || from > to || to > fs_state.disk_header->disk_size) {
        return;
    }

    for (unsigned long i = from; i < to; i++) {
        fs_state.disk[i] = 0;
    }
}

FSFILE* allocate_file(const char* path, int file_type) {
    if (!is_initialized()) {
        return NULL;
    }

    assert((file_type > T_NONE) && (file_type < T_END));

    FSFILE* dir = get_ptr(fs_state.disk_header->current_directory);

    unsigned long id = hash2(path) + file_type;
    unsigned long empty_slot_addr = 0;
    if ((find_file(id, NULL, &empty_slot_addr, file_type))) {
        error(COLOR_MESSAGE "'%s'" NONE " File already exists\n", path);
        return NULL;
    }

    FSFILE* file = allocate(TOTAL_FILE_HEADER_SIZE);

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
                fs_write(&file_addr, sizeof(unsigned long), dir);
        }
    }

    return file;
}

// return -1 on failure
// FIX(lucas): deallocate blocks/deallocate files isn't working properly
int deallocate_file(FSFILE* file) {
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

// Mark blocks as free
int deallocate_blocks(unsigned long addr) {
    if (!can_access_address(addr)) {
        return -1;
    }
    
    struct Data_block* block = get_ptr(addr);
    unsigned long next = block->next;

    // flush(addr, addr + TOTAL_BLOCK_SIZE);
    int err = free_block(addr, TOTAL_BLOCK_SIZE, BLOCK_USED);
    if (err != 0) {
        return err;
    }
    // block->block_type = BLOCK_FREE;
    
    if (can_access_address(next)) {
        deallocate_blocks(next);
        // block->next = 0;
    }
    return 0;
}

// Implement smarter deletion of files (current deletion is lazy)
int remove_file(const char* path, int file_type) {
    unsigned long id = hash2(path) + file_type;
    unsigned long location = 0;
    FSFILE* file = find_file(id, &location, NULL, file_type);
    if (!file || !location) {
        error(COLOR_MESSAGE "'%s'" NONE " No such file or directory\n", path);
        return -1;
    }
    unsigned long* file_addr = get_ptr(location);
    assert(get_absolute_address(file) == *file_addr);
    
    // To make sure you don't delete the directory you are in
    if (*file_addr == fs_state.disk_header->current_directory || *file_addr == fs_state.disk_header->root_directory) {
        error("Can't remove this directory\n");
        return -1;
    }
    if (deallocate_file(file) != 0) {
        return -1;
    }

    if (!file_addr) {
        error(COLOR_MESSAGE "'%s'" NONE " Failed to remove file\n");
        return -1;
    }
    if (free_block(*file_addr, TOTAL_FILE_HEADER_SIZE, BLOCK_FILE_HEADER) != 0) {
        return -1;
    }
    fslog("Removed file '%s' (%lu)\n", path, *file_addr);
    *file_addr = 0;
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
 
FSFILE* find_file(unsigned long id, unsigned long* location, unsigned long* empty_slot, int file_type) {
    if (!is_initialized()) {
        return NULL;
    }

    FSFILE* dir = get_ptr(fs_state.disk_header->current_directory);

    if (!dir) {
        return NULL;
    }

    struct Data_block* block = NULL;

    unsigned long next = dir->first_block;
    if (next == 0)
        return NULL;

    FSFILE* file = NULL;
    while ((block = read_block(next)) != NULL) {
        for (unsigned long i = 0; i < block->bytes_used; i += (sizeof(unsigned long))) {
            unsigned long addr = *(unsigned long*)(&block->data[i]);
            if (addr == 0) {
                if (empty_slot) *empty_slot = get_absolute_address(&block->data[i]);
                continue;
            }
            file = get_ptr(addr);
            if (file) {
                if (file->id == id) {
                    if (location != NULL) {
                        *location = get_absolute_address(&block->data[i]);
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

struct Data_block* read_block(unsigned long block_addr) {
    struct Data_block* block = (struct Data_block*)get_ptr(block_addr);
    if (!block) {
        return NULL;
    }
    return block;
}

struct Data_block* get_last_block(struct Data_block* block) {
    if (!block) {
        return NULL;
    }
    if (block->next == 0) {
        return block;
    }
    struct Data_block* next = (struct Data_block*)get_ptr(block->next);
    if (next) {
        return get_last_block(next);
    }
    return NULL;
}

// Different cases:
// - When bytes_used != 0
// - When the size is less than BLOCK_SIZE - bytes_used
// - When the size is greater than BLOCK_SIZE - bytes_used but is less than BLOCK_SIZE
void write_to_blocks(FSFILE* file, const void* data, unsigned long size, unsigned long* bytes_written, unsigned long block_addr) {
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

void read_file_contents(unsigned long block_addr, FILE* output) {
    if (!is_initialized())
        return;

    if (!can_access_address(block_addr))
        return;

    if (!output)
        output = stdout;

    struct Data_block* block = get_ptr(block_addr);
    if (!block)
        return;

    fprintf(output, "%.*s", block->bytes_used, block->data);
    
    if (block->next == 0) {
        fprintf(output, "\n");
        return;
    }
    read_file_contents(block->next, output);
}

int read_dir_contents(const FSFILE* file, unsigned long block_addr, FILE* output) {
    if (!can_access_address(block_addr)) {
        return -1;
    }
    if (!output || !is_initialized()) {
        return -1;
    }

    if (file->type != T_DIR) {
        error(COLOR_MESSAGE "'%s'" NONE ": File is not a directory\n", file->name);
        return -1;
    }

    struct Data_block* block = get_ptr(block_addr);

    int count = 0;
    for (unsigned long i = 0; i < block->bytes_used; i += (sizeof(unsigned long)), count++) {
        unsigned long addr = *(unsigned long*)(&block->data[i]);
        if (addr == 0)
            continue;
        
        FSFILE* file_in_dir = get_ptr(addr);
        if (file_in_dir) {
            fprintf(output, "%-7lu %i %7i ", addr, file_in_dir->type, file_in_dir->size);
            if (file_in_dir->type == T_DIR) {
                fprintf(output, COLOR_PATH "%s/", file_in_dir->name);
            }
            else if (file_in_dir->type == T_FILE) {
                fprintf(output, RED "%s", file_in_dir->name);
            }
            else {
                fprintf(output, "%s", file_in_dir->name);
            }
            fprintf(output, NONE "\n");
        }
    }

    if (block->next == 0)
        return 0;
    
    return read_dir_contents(file, block->next, output);;
}

int count_blocks(struct Data_block* block) {
    if (!block) {
        return 0;
    }
    if (block->next == 0) {
        return 1;
    }
    struct Data_block* next = (struct Data_block*)get_ptr(block->next);
    if (next) {
        return count_blocks(next) + 1;
    }
    return 0;
}

int get_size_of_blocks(unsigned long block_addr) {
    if (block_addr == 0 || block_addr == ULONG_MAX) {
        return 0;
    }
    struct Data_block* block = get_ptr(block_addr);
    if (block->next == 0) {
        return block->bytes_used;
    }

    return get_size_of_blocks(block->next) + block->bytes_used;
}

void* get_ptr(unsigned long address) {
    if (!is_initialized() || !fs_state.disk) {
        return NULL;
    }
    if (can_access_address(address)) {
        return (void*)fs_state.disk + address;
    }
    return NULL;
}

// Addresses == 0 are invalid
unsigned long get_absolute_address(void* address) {
    if (!is_initialized() || !fs_state.disk) {
        return 0;
    }
    return (unsigned long)address - (unsigned long)fs_state.disk;
}

int can_access_address(unsigned long address) {
    if (!is_initialized())
        return 0;
    if (address == 0 || address >= fs_state.disk_header->disk_size)
        return 0;

    return 1;
}


int fs_init(unsigned long disk_size) {
    // Mute warnings
    (void)get_size_of_blocks;
    (void)print_block_info;
    (void)deallocate_blocks;

    if (is_initialized()) {
        return -1;
    }
    
    fs_state.disk = calloc(disk_size, sizeof(char));
    if (!fs_state.disk) {
        error("%s: Failed to allocate memory for disk\n", __FUNCTION__);
        return -1;
    }

    return initialize(&fs_state, disk_size);;
}

int fs_init_from_disk(const char* path) {
    if (is_initialized()) {
        fs_free();
    }
    char* disk = read_file(path);
    if (!disk) {
        error("Failed to allocate memory for disk\n");
        return -1;
    }
    fs_state.is_initialized = 1;
    fs_state.error = 0;
    fs_state.err = NULL; //fopen("./log/error.log", "w+");
    fs_state.log = fopen(DATA_PATH "/log/disk_events.log", "ab");

    fs_state.disk = disk;
    fs_state.disk_header = (struct FS_disk_header*)fs_state.disk;
    if (fs_state.disk_header->magic != HEADER_MAGIC) {
        error("Failed to load disk. Invalid header magic (is: " COLOR_NUMBERS "%i" NONE ", should be: " COLOR_NUMBERS "%i" NONE ").\n", fs_state.disk_header->magic, HEADER_MAGIC);
        return -1;
    }
    return 0;
}

FSFILE* fs_open(const char* path, const char* mode) {
    if (!is_initialized()) {
        return NULL;
    }
    if (*mode != 'r' && *mode != 'w' && *mode != 'a') {
        return NULL;
    }
    FSFILE* file = NULL;
    int file_type = T_FILE;
    unsigned long id = hash2(path) + file_type;

    switch (*mode) {
        case 'w': {

            if ((file = find_file(id, NULL, NULL, file_type))) {
                if (file->type == T_FILE) {
                    deallocate_file(file);
                    file->mode = MODE_WRITE;
                }
                return file;
            }
            else {
                file = allocate_file(path, T_FILE);
                if (file) {
                    file->mode = MODE_WRITE;
                    return file;
                }
                error(COLOR_MESSAGE "'%s'" NONE ": Failed to create file\n", path);
                return NULL;
            }
        }
            break;

        case 'r': {
            FSFILE* file = find_file(id, NULL, NULL, file_type);
            if (!file) {
                error(COLOR_MESSAGE "'%s'" NONE ": No such file\n", path);
                return NULL;
            }
            file->mode = MODE_READ;
            return file;
        
        }
            break;

        case 'a': {
            FSFILE* file = find_file(id, NULL, NULL, file_type);
            if (!file) {
                error(COLOR_MESSAGE "'%s'" NONE ": No such file\n", path);
                return NULL;
            }
            file->mode = MODE_APPEND;
            return file;
        }
            break;

        default:
            break;
    }
    error("Open file failed\n");
    return NULL;
}

FSFILE* fs_open_dir(const char* path) {
    if (!is_initialized()) {
        return NULL;
    }
    int file_type = T_DIR;
    unsigned long id = hash2(path) + file_type;
    FSFILE* file = find_file(id, NULL, NULL, file_type);
    if (!file) {
        error(COLOR_MESSAGE "'%s'" NONE ": No such directory\n", path);
        return NULL;
    }
    return file;
}

FSFILE* fs_create_dir(const char* path) {
    if (!is_initialized()) {
        return NULL;
    }

    FSFILE* file = allocate_file(path, T_DIR);
    if (file) {
        unsigned long addr = get_absolute_address(file);
        fs_write(&addr, sizeof(unsigned long), file);   // self
        fs_write(&fs_state.disk_header->current_directory, sizeof(unsigned long), file);   // parent
        return file;
    }
    error(COLOR_MESSAGE "'%s'" NONE ": Failed to create directory\n", path);
    return NULL;
}

// Change current directory
int fs_change_dir(const char* path) {
    if (!is_initialized()) {
        return -1;
    }
    FSFILE* dir = fs_open_dir(path);
    if (!dir) {
        return -1;
    }
    fs_state.disk_header->current_directory = get_absolute_address(dir);
    return 0;
}


int fs_remove_file(const char* path) {
    return remove_file(path, T_FILE);
}

int fs_remove_dir(const char* path) {
    return remove_file(path, T_DIR);
}

void fs_close(FSFILE* file) {
    if (!file) {
        return;
    }
    if (file->mode != 0) {
        file->mode = 0;
    }
}

// TODO(lucas): Cleanup!
int fs_write(const void* data, unsigned long size, FSFILE* file) {
    if (!file || !is_initialized()) {
        return -1;
    }

    if ((MODE_WRITE != (file->mode & MODE_WRITE) && MODE_APPEND != (file->mode & MODE_APPEND)) && file->type != T_DIR) {
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

void fs_print_file_info(const FSFILE* file, FILE* output) {
    if (!file || !output) {
        return;
    }
    
    fprintf(output, "name: %s\n", file->name);
    fprintf(output, "type: %i\n", file->type);
    fprintf(output, "mode: %i\n", file->mode);
    fprintf(output, "first block: %lu\n", file->first_block);
    fprintf(output, "header addr: %lu\n", get_absolute_address((void*)file));
}

int fs_read(const FSFILE* file, FILE* output) {
    if (!file || !output || !is_initialized())
        return 0;

    if (file->first_block == 0)
        return 0;

    if (file->type == T_DIR) {
        error(COLOR_MESSAGE "'%s/'" NONE ": Not a regular file\n", file->name);
        return -1;
    }

    read_file_contents(file->first_block, output);
    return 0;
}

int fs_list(const FSFILE* file, FILE* output) {
    if (!output || !is_initialized()) {
        return -1;
    }

    if (!file) {
        file = get_ptr(fs_state.disk_header->current_directory);
        if (!file) {
            error("Current directory isn't set\n");
            return -1;
        }
    }

    return read_dir_contents(file, file->first_block, output);
}

void fs_dump_disk(const char* path) {
    if (!is_initialized()) {
        return;
    }

    FILE* file = fopen(path, "w");
    if (file) {
        fwrite(fs_state.disk, sizeof(char), fs_state.disk_header->disk_size, file);
        fclose(file);
    }
}

int fs_get_error() {
    if (!is_initialized()) {
        error("%s\n", "File system is not initialized");
        return -1;
    }
    if (!fs_state.error)
        return 0;       // Okay, no error
    fs_state.error = 0; // Error has occured, reset it so that the error is not persistent upon next get_error() call
    if (!fs_state.err)
        return -1;
    char* contents = read_open_file(fs_state.err);
    if (contents) {
        printf("%s", contents);
        free(contents);
    }
    return -1;
}

void fs_free() {
    if (fs_state.is_initialized) {
        if (fs_state.disk) {
            free(fs_state.disk);
            fs_state.disk = NULL;
        }
        if (fs_state.err) fclose(fs_state.err);
        if (fs_state.log) fclose(fs_state.log);
        fs_state.disk_header->current_directory = 0;
        fs_state.disk_header = NULL;
        fs_state.is_initialized = 0;
    }
}

// This test function is no longer working due to change of paths
void fs_test() {
    unsigned long disk_size = DEFAULT_DISK_SIZE;
    assert(fs_init(disk_size) == 0);

    (void)count_blocks;

    fs_create_dir("projects");
    fs_create_dir("logs");
    fs_create_dir("shared");

    const char name[] = "config.c";
    {
        FSFILE* file = fs_open(name, "w");
        assert(file != NULL);
        const char str[] = "// config.c\n\ninclude <stdio.h>\ninclude <stdlib.h>\n\n";
        fs_write(str, strlen(str), file);
        fs_close(file);
    }

    {
        const char name[] = "hash.c";
        FSFILE* file = fs_open(name, "w");
        assert(file != NULL);
        char* str = read_file("hash.c");
        if (str) {
            fs_write(str, strlen(str), file);
            free(str);
        }
        fs_close(file);
    }

    {
        FSFILE* file = find_file(hash2(name), NULL, NULL, T_FILE);
        assert(file != NULL);
    }
    {
        FSFILE* file = fs_open("config.c", "r");
        if (file) {
            fs_close(file);
        }
    }

    const char disk_path[] = "./data/test.disk";
    fs_dump_disk(disk_path);
    fs_init_from_disk(disk_path);
    assert(is_initialized());

    fs_free();
    assert(!is_initialized());
    assert(fs_state.disk == NULL);
}