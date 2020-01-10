// block.c

#include <stdlib.h>
#include <stdio.h>

#include "block.h"

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
