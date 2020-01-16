// dir.c

#include "file_system.h"
#include "block.h"
#include "file.h"
#include "dir.h"

typedef struct FSFILE FSFILE;

static struct FSFILE* get_parent_dir(const FSFILE* dir);

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
	return NULL;
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

// Get relative path directory
struct FSFILE* get_path_dir(const char* path) {
    struct FSFILE* dir = get_ptr(get_state()->disk_header->current_directory);
    
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
    		dir = get_ptr(get_state()->disk_header->current_directory);
    		i++;
    		continue;
    	}
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