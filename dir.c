// dir.c

#include "file_system.h"
#include "block.h"
#include "file.h"
#include "dir.h"

typedef struct FSFILE FSFILE;

static struct FSFILE* get_parent_dir(const FSFILE* dir);
static struct FSFILE* get_current_dir();
static int pwd(const FSFILE* current, FILE* output);

struct FSFILE* get_parent_dir(const FSFILE* dir) {
	assert(dir != NULL);
	struct Data_block* block = read_block(dir->first_block);
	if (!block) {
		error(COLOR_MESSAGE "'%s'" NONE ": Directory is empty\n", dir->name);
		return NULL;
	}
	if (block->bytes_used >= (2 * sizeof(addr_t))) {
		addr_t* addr = (addr_t*)block->data;
		return get_ptr(addr[1]);
	}
	error(COLOR_MESSAGE "'%s'" NONE ": Directory is empty\n", dir->name);
	return NULL;
}

struct FSFILE* get_current_dir() {
	return get_ptr(get_state()->disk_header->current_directory);
}

int pwd(const FSFILE* current, FILE* output) {
	const struct FSFILE* parent = get_parent_dir(current);
	if (parent != current) {
		pwd(parent, output);
	}
	if (current->type == T_DIR)
		fprintf(output, "/");
	fprintf(output, "%s", current->name);

	return 0;
}

int read_dir_contents(const struct FSFILE* file, unsigned long block_addr, int iteration, FILE* output) {
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
        
        struct FSFILE* to_print = get_ptr(addr[i]);
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

// Get relative or absolute path directory (and file if supplied)
// Assign the last file in the path to file (struct FSFILE** file)
// Pass in NULL if that isn't needed
struct FSFILE* get_path_dir(const char* path, struct FSFILE** file) {
    struct FSFILE* dir = get_ptr(get_state()->disk_header->current_directory);
    if (!file)
    	file = &dir;

    if (!dir) {
        return NULL;
    }

    char file_name[FILE_NAME_SIZE] = {0};
    int index = 0;

    unsigned long path_length = strlen(path);
    for (int i = 0; i < path_length + 1 && index < FILE_NAME_SIZE; i++) {
    	if (path[i] == '/' && i == 0) {
    		dir = get_ptr(get_state()->disk_header->root_directory);
    		continue;
    	}
    	if (path[i] == '.' && index == 0) {
    		// '..' The user wants to access the parent directory
    		// Read the second file of the directory
    		if (path[i + 1] == '.') {
    			dir = get_parent_dir(dir);
    			i += 2;
    			continue;
    		}
			dir = get_current_dir();
			i++;
    		continue;
    	}
        if (path[i] == '/' || i == path_length) {
            if (index == 0 && i == path_length) {
                return dir;
            }
            unsigned long id = hash(file_name, index);
            *file = find_file(dir, id, NULL, NULL);
            if (!*file) {
                return NULL;
            }
            if (is_dir(*file)) {
            	dir = *file;
            }
            index = 0;
            continue;
        }
        file_name[index++] = path[i];
    }

    return dir;
}

int print_working_directory(FILE* output) {
	if (!is_initialized()) return -1;
	const struct FSFILE* current = get_current_dir();
	if (!current)
		return -1;

	fprintf(output, COLOR_PATH);
	int result = pwd(current, output);
	fprintf(output, "\n" NONE);
	return result;
}

int is_dir(const struct FSFILE* file) {
	return file->type == T_DIR;
}

int can_remove_dir(const struct FSFILE* file) {
	assert(file != NULL);

	if (!is_dir(file)) {
		error(COLOR_MESSAGE "'%s'" NONE ": File is not a directory\n", file->name);
		return 0;
	}

	struct Data_block* block = read_block(file->first_block);
	assert(block != NULL);	// Directories can't be empty
	return block->bytes_used == (sizeof(addr_t) * 2);
}

// UNUSED
int fill_empty_file_slots(struct Data_block* block, int from_index) {
	assert(block != NULL);

	addr_t*
	for (int i = 0; i < block->bytes_used; i++) {

	}

	return 0;
}