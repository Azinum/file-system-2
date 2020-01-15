// fs2.c
// tab size: 4

#include <stdio.h>
#include <stdlib.h>

#include "fs2.h"
#include "file_system.h"
#include "file.h"
#include "hash.h"
#include "read.h"
#include "block.h"
#include "alloc.h"

static int initialize(struct FS_state* state, unsigned long disk_size);

static int remove_file(const char* path, int file_type);
static void read_file_contents(unsigned long block_addr, FILE* output);
static int read_dir_contents(const FSFILE* file, unsigned long block_addr, int iteration, FILE* output);
static FSFILE* get_path_dir(const char* path);

int initialize(struct FS_state* state, unsigned long disk_size) {
    if (!state) {
        return -1;
    }
    state->is_initialized = 1;
    state->error = 0;
    state->err = NULL;
    state->log = NULL;

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

int remove_file(const char* path, int file_type) {
    unsigned long id = hash2(path);
    unsigned long location = 0;
    FSFILE* file = find_file(NULL, id, &location, NULL);
    if (!file || !location) {
        error(COLOR_MESSAGE "'%s'" NONE " No such file or directory\n", path);
        return -1;
    }
    unsigned long* file_addr = get_ptr(location);
    assert(get_absolute_address(file) == *file_addr);
    
    // To make sure you don't delete the directory you are in
    if (*file_addr == get_state()->disk_header->current_directory || *file_addr == get_state()->disk_header->root_directory) {
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

int read_dir_contents(const FSFILE* file, unsigned long block_addr, int iteration, FILE* output) {
    if (!can_access_address(block_addr)) {
        return -1;
    }
    if (!output || !is_initialized()) {
        return -1;
    }

    if (file->type != T_DIR) {
        error(COLOR_MESSAGE "'%s'" NONE ": Not a directory\n", file->name);
        return -1;
    }

    struct Data_block* block = get_ptr(block_addr);
    unsigned long* addr = (unsigned long*)block->data;

    for (int i = 0; i < block->bytes_used / (sizeof(unsigned long)); i++, iteration++) {
        if (addr[i] == 0)
            continue;
        
        FSFILE* to_print = get_ptr(addr[i]);
        if (to_print) {
            fprintf(output, "%-7lu %i %7i ", addr[i], to_print->type, to_print->size);

            if (iteration == 0) {
                printf(COLOR_PATH ".\n" NONE);
                continue;
            }
            else if (iteration == 1) {
                printf(COLOR_PATH "..\n" NONE);
                continue;
            }

            if (to_print->type == T_DIR)
                fprintf(output, COLOR_PATH "%s/" NONE, to_print->name);
            else if (to_print->type == T_FILE)
                fprintf(output, COLOR_FILE "%s" NONE, to_print->name);
            else
                fprintf(output, "%s", to_print->name);
            
            fprintf(output, "\n");
        }
    }

    if (block->next == 0)
        return 0;
    
    return read_dir_contents(file, block->next, iteration, output);
}

// Get relative path directory
FSFILE* get_path_dir(const char* path) {
    FSFILE* dir = get_ptr(get_state()->disk_header->current_directory);
    
    if (!dir) {
        return NULL;
    }

    char file_name[FILE_NAME_SIZE] = {0};
    int index = 0;

    unsigned long path_length = strlen(path);
    for (int i = 0; i < path_length+1 && index < FILE_NAME_SIZE; i++) {
        if (path[i] == '/' || i == path_length) {
            if (index == 0 && i == path_length) {
                return dir;
            }
            unsigned long id = hash(file_name, index);
            dir = find_file(dir, id, NULL, NULL);
            if (!dir) {
                return NULL;
            }
            if (dir->type != T_DIR) {
                return NULL;
            }
            index = 0;
            continue;
        }
        file_name[index++] = path[i];
    }

    return dir;
}


int fs_init(unsigned long disk_size) {
    // Mute warnings
    (void)get_size_of_blocks;
    (void)print_block_info;
    (void)deallocate_blocks;

    if (is_initialized()) {
        return -1;
    }
    
    get_state()->disk = calloc(disk_size, sizeof(char));
    if (!get_state()->disk) {
        error("%s: Failed to allocate memory for disk\n", __FUNCTION__);
        return -1;
    }

    return initialize(get_state(), disk_size);;
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
    get_state()->is_initialized = 1;
    get_state()->error = 0;
    get_state()->err = NULL; //fopen("./log/error.log", "w+");
    get_state()->log = fopen(DATA_PATH "/log/disk_events.log", "ab");

    get_state()->disk = disk;
    get_state()->disk_header = (struct FS_disk_header*)get_state()->disk;
    if (get_state()->disk_header->magic != HEADER_MAGIC) {
        error("Failed to load disk. Invalid header magic (is: " COLOR_NUMBERS "%i" NONE ", should be: " COLOR_NUMBERS "%i" NONE ").\n", get_state()->disk_header->magic, HEADER_MAGIC);
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

    unsigned long id = hash2(path);

    switch (*mode) {
        case 'w': {

            if ((file = find_file(NULL, id, NULL, NULL))) {
                if (file->type != T_FILE) {
                    error(COLOR_MESSAGE "'%s'" NONE ": No such file\n", path);
                    return NULL;
                }
                deallocate_file(file);
                file->mode = MODE_WRITE;
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
            FSFILE* file = find_file(NULL, id, NULL, NULL);
            if (!file) {
                error(COLOR_MESSAGE "'%s'" NONE ": No such file\n", path);
                return NULL;
            }
            file->mode = MODE_READ;
            return file;
        
        }
            break;

        case 'a': {
            FSFILE* file = find_file(NULL, id, NULL, NULL);
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
    unsigned long id = hash2(path);
    FSFILE* file = find_file(NULL, id, NULL, NULL);
    if (!file) {
        error(COLOR_MESSAGE "'%s'" NONE ": No such directory\n", path);
        return NULL;
    }
    if (file->type != T_DIR) {
        error(COLOR_MESSAGE "'%s'" NONE ": Not a directory\n", path);
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
        unsigned long current = get_state()->disk_header->current_directory;
        fs_write(current ? &current : &addr,  sizeof(unsigned long), file);   // parent
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

    FSFILE* dir = get_path_dir(path);
    if (!dir) {
        error(COLOR_PATH "'%s'" NONE " Invalid path\n", path);
        return -1;
    }
    get_state()->disk_header->current_directory = get_absolute_address(dir);
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

int fs_write(const void* data, unsigned long size, FSFILE* file) {
    return write_data(data, size, file);
}

void fs_print_file_info(const FSFILE* file, FILE* output) {
    if (!file || !output) {
        return;
    }
    unsigned long addr = get_absolute_address((void*)file);

    fprintf(output, "%-7lu %i %7i ", addr, file->type, file->size);
    if (file->type == T_DIR) {
        fprintf(output, COLOR_PATH "%s/", file->name);
    }
    else if (file->type == T_FILE) {
        fprintf(output, COLOR_FILE "%s", file->name);
    }
    else {
        fprintf(output, "%s", file->name);
    }
    fprintf(output, NONE "\n");
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
        file = get_ptr(get_state()->disk_header->current_directory);
        if (!file) {
            error("Current directory isn't set\n");
            return -1;
        }
    }

    return read_dir_contents(file, file->first_block, 0, output);
}

void fs_dump_disk(const char* path) {
    if (!is_initialized()) {
        return;
    }

    FILE* file = fopen(path, "w");
    if (file) {
        fwrite(get_state()->disk, sizeof(char), get_state()->disk_header->disk_size, file);
        fclose(file);
    }
}

int fs_get_error() {
    if (!is_initialized()) {
        error("%s\n", "File system is not initialized");
        return -1;
    }
    if (!get_state()->error)
        return 0;       // Okay, no error
    get_state()->error = 0; // Error has occured, reset it so that the error is not persistent upon next get_error() call
    if (!get_state()->err)
        return -1;
    char* contents = read_open_file(get_state()->err);
    if (contents) {
        printf("%s", contents);
        free(contents);
    }
    return -1;
}

void fs_free() {
    !get_state()->is_initialized ? error("Failed to free state (it's already been free'd)\n") : (void)0;

    if (get_state()->is_initialized) {
        if (get_state()->disk) {
            free(get_state()->disk);
            get_state()->disk = NULL;
        }
        if (get_state()->err) fclose(get_state()->err);
        if (get_state()->log) fclose(get_state()->log);
        get_state()->disk_header = NULL;
        get_state()->is_initialized = 0;
    }
}