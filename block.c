// block.c

#include <stdlib.h>
#include <stdio.h>

#include "block.h"
#include "file_system.h"

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
	if (!can_access_address(block_addr)) {
		return -1;
	}
    struct Data_block* block = get_ptr(block_addr);
    if (block->next == 0) {
        return block->bytes_used;
    }

    return get_size_of_blocks(block->next) + block->bytes_used;
}

struct Data_block* read_block(unsigned long block_addr) {
    struct Data_block* block = (struct Data_block*)get_ptr(block_addr);
    if (!block) {
        return NULL;
    }
    assert(block->block_type > BLOCK_NONE && block->block_type < BLOCK_TYPES_COUNT);
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