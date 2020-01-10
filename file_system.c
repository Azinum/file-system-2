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

#include "file_system.h"
#include "file.h"
#include "hash.h"
#include "read.h"

#define HEADER_MAGIC 0xbeefaaaa

#define COLORING 1

#if COLORING
#define RED "\e[1;31m"
#define BLUE "\e[1;34m"
#define NONE "\e[0m"
#define DARK_GREEN "\e[0;32m"
#else
#define RED ""
#define BLUE ""
#define DARK_GREEN ""
#endif

struct FS_disk_header {
    int magic;
    unsigned long disk_size;
    unsigned long root_directory;
};

struct FS_state {
    char* disk;
    int is_initialized;
    FILE* err;
    FILE* log;
    struct FS_disk_header* disk_header;
    FSFILE* current_directory;
};

static struct FS_state fs_state;

static int is_initialized();
static void error(char* format, ...);
static void fslog(char* format, ...);

static int initialize(struct FS_state* state, unsigned long disk_size);

static struct Data_block* allocate_blocks(int count);
static void flush(unsigned long from, unsigned long to);
static void* allocate(unsigned long size);
static FSFILE* allocate_file(const char* path, int file_type);
static int deallocate_file(FSFILE* file);
static void deallocate_blocks(struct Data_block* block);
static FSFILE* find_file(unsigned long hashed_name, unsigned long* position);
static struct Data_block* read_block(unsigned long block_addr);
static struct Data_block* get_last_block(struct Data_block* block);
static void write_to_blocks(FSFILE* file, const void* data, unsigned long size, unsigned long* bytes_written, unsigned long block_addr);
static void read_file_contents(unsigned long block_addr, FILE* output);
static void read_dir_contents(FSFILE* file, unsigned long block_addr, FILE* output);
static void print_block_info(struct Data_block* block, FILE* output);
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
    if (!is_initialized())
        return;

    va_list args;
    va_start(args, format);

    if (fs_state.err) {
        vfprintf(fs_state.err, format, args);
    }
    else {
        vfprintf(stderr, format, args);
    }
    
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
    state->err = NULL;
    state->log = fopen("log/disk_events.log", "ab");

    state->disk_header = (struct FS_disk_header*)state->disk;
    state->disk_header->magic = HEADER_MAGIC;
    state->disk_header->disk_size = sizeof(char) * disk_size;
    FSFILE* root = fs_create_dir("root");
    if (!root) {
        error("Failed to create root directory\n");
        return -1;
    }
    state->current_directory = root;
    state->disk_header->root_directory = get_absolute_address(root);
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

    FSFILE* dir = fs_state.current_directory;

    unsigned long hashed_name = hash2(path);

    FSFILE* f;
    if ((f = find_file(hashed_name, NULL))) {
        return NULL;
    }

    FSFILE* file = allocate(TOTAL_FILE_HEADER_SIZE);

    if (file) {
        file->block_type = BLOCK_FILE_HEADER;
        strncpy(file->name, path, FILE_NAME_SIZE);
        file->hashed_name = hashed_name;
        file->type = file_type;
        file->first_block = 0;

        if (dir) {
            unsigned long file_addr = get_absolute_address(file);
            fs_write(&file_addr, sizeof(unsigned long), dir);
        }
    }

    return file;
}

// return -1 on failure
int deallocate_file(FSFILE* file) {
    if (!is_initialized() || !file) {
        return -1;
    }

    if (!can_access_address(file->first_block)) {
        return 0;
    }

    struct Data_block* block = get_ptr(file->first_block);
    if (block) {
        file->first_block = 0;
        file->size = 0;
        //deallocate_blocks(block);
        return 0;
    }
    return -1;
}

// Mark blocks as free
// This function might be the source of the @BUG
void deallocate_blocks(struct Data_block* block) {
    if (!block) {
        return;
    }
    
    block->block_type = BLOCK_FREE;

    if (block->next != 0) {
        struct Data_block* next = (struct Data_block*)get_ptr(block->next);
        if (next) {
            block->next = 0;
            deallocate_blocks(next);
        }
    }
}
 
FSFILE* find_file(unsigned long hashed_name, unsigned long* location) {
    if (!is_initialized()) {
        return NULL;
    }

    FSFILE* dir = fs_state.current_directory;

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
                continue;
            }
            file = (FSFILE*)get_ptr(addr);
            if (file) {
                if (file->hashed_name == hashed_name) {
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

void read_dir_contents(FSFILE* file, unsigned long block_addr, FILE* output) {
    if (!can_access_address(block_addr)) {
        return;
    }
    if (!output || !is_initialized()) {
        return;
    }

    struct Data_block* block = get_ptr(block_addr);

    for (unsigned long i = 0; i < block->bytes_used; i += (sizeof(unsigned long))) {
        unsigned long addr = *(unsigned long*)(&block->data[i]);
        if (addr == 0)
            continue;
        
        FSFILE* file_in_dir = get_ptr(addr);
        if (file_in_dir) {
            fprintf(output, "%-7lu %i %7i ", addr, file_in_dir->type, file_in_dir->size);
            if (file_in_dir->type == T_DIR) {
                fprintf(output, BLUE "%s/", file_in_dir->name);
            }
            else if (file_in_dir->type == T_FILE) {
                fprintf(output, RED "%s", file_in_dir->name);
            }
            else {
                fprintf(output, "%s", file_in_dir->name);
            }

            fprintf(output, "\n" NONE);
        }
    }

    if (block->next == 0)
        return;
    
    read_dir_contents(file, block->next, output);
}

void print_block_info(struct Data_block* block, FILE* output) {
    if (!block) {
        fprintf(output, "Invalid block (block is NULL)\n");
        return;
    }
    char* block_info[] = {
        "-",
        "BLOCK_USED",
        "BLOCK_FREE"
    };
    fprintf(output,
        "struct Data_block {\n"
        "   type: %s\n"
        "   data: '%.*s...'\n"
        "   bytes_used: %i\n"
        "   next: %lu\n"
        "}\n"
        ,
        block_info[(int)block->block_type],
        7,
        block->data,
        block->bytes_used,
        block->next
    );
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
    if (address > 0 && address < fs_state.disk_header->disk_size) {
        return (void*)&fs_state.disk[address];
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
    if (address == 0 || address > fs_state.disk_header->disk_size)
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
    fs_state.current_directory = NULL;
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
    fs_state.err = fopen("log/error.log", "w+");
    fs_state.log = NULL;//fopen("log/disk_events.log", "ab");

    fs_state.disk = disk;
    fs_state.disk_header = (struct FS_disk_header*)fs_state.disk;
    if (fs_state.disk_header->magic != HEADER_MAGIC) {
        error("Failed to load disk. Invalid header magic (is: " BLUE "%i" NONE ", should be: " BLUE "%i" NONE ").\n", fs_state.disk_header->magic, HEADER_MAGIC);
        return -1;
    }
    fs_state.current_directory = NULL;
    if (can_access_address(fs_state.disk_header->root_directory)) {
        fs_state.current_directory = get_ptr(fs_state.disk_header->root_directory);
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
    unsigned long hashed = hash2(path);

    switch (*mode) {
        case 'w': {

            if ((file = find_file(hashed, NULL))) {
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
                error(DARK_GREEN "'%s'" NONE ": Failed to create file\n", path);
                return NULL;
            }
        }
            break;

        case 'r': {
            FSFILE* file = find_file(hashed, NULL);
            if (!file) {
                error(DARK_GREEN "'%s'" NONE ": No such file\n", path);
                return NULL;
            }
            file->mode = MODE_READ;
            return file;
        
        }
            break;

        case 'a': {
            FSFILE* file = find_file(hashed, NULL);
            if (!file) {
                error(DARK_GREEN "'%s'" NONE ": No such file\n", path);
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

FSFILE* fs_create_dir(const char* path) {
    if (!is_initialized()) {
        return NULL;
    }

    FSFILE* file = allocate_file(path, T_DIR);
    if (file) {
        unsigned long addr = get_absolute_address(file);
        fs_write(&addr, sizeof(unsigned long), file);
        return file;
    }
    error(DARK_GREEN "'%s'" NONE ": Failed to create directory\n", path);
    return NULL;
}

int fs_remove_file(const char* path) {
    unsigned long hashed = hash2(path);
    unsigned long location = 0;
    FSFILE* file = find_file(hashed, &location);
    if (!file) {
        error(DARK_GREEN "'%s'" NONE " No such file or directory\n", path);
        return -1;
    }
    if (location == 0) {
        return -1;
    }

    file->block_type = BLOCK_FILE_HEADER_FREE;

    if (deallocate_file(file) != 0) {
        return -1;
    }
    file->first_block = 0;
    unsigned long* loc = get_ptr(location);
    fslog("Removed file '%s' (loc: %lu)\n", path, *loc);
    assert(get_absolute_address(file) == *loc);

    if (loc) {
        *loc = 0;
    }
    return 0;
}

void fs_close(FSFILE* file) {
    if (!file) {
        return;
    }
    if (file->mode != 0) {
        file->mode = 0;
    }
}

// @TODO: Cleanup!
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

void fs_read(const FSFILE* file, FILE* output) {
    if (!file || !output || !is_initialized()) {
        return;
    }
    if (file->first_block == 0) {
        return;
    }

    read_file_contents(file->first_block, output);
}

void fs_list(FILE* output) {
    if (!output || !is_initialized()) {
        return;
    }

    FSFILE* file = fs_state.current_directory;

    if (!file) {
        return;
    }

    read_dir_contents(file, file->first_block, output);
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

void fs_get_error() {
    if (!is_initialized()) {
        printf("%s\n", "File system is not initialized");
        return;
    }
    if (!fs_state.err) {
        return;
    }
    char* contents = read_open_file(fs_state.err);
    if (contents) {
        printf("%s", contents);
        free(contents);
    }
}

void fs_free() {
    if (fs_state.is_initialized) {
        if (fs_state.disk) {
            free(fs_state.disk);
            fs_state.disk = NULL;
        }
        if (fs_state.err) fclose(fs_state.err);
        if (fs_state.log) fclose(fs_state.log);
        fs_state.current_directory = NULL;
        fs_state.disk_header = NULL;
        fs_state.is_initialized = 0;
    }
}

void fs_test() {
    unsigned long disk_size = 1024 << 3;
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
        FSFILE* file = find_file(hash2(name), NULL);
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