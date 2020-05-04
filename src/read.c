// read.c

#include <stdlib.h>
#include <stdio.h>

#include "read.h"

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    
    return read_open_file(file);
}

char* read_open_file(FILE* file) {
    if (!file) {
        return NULL;
    }
    long buffer_size, read_size;
    char* buffer = NULL;

    fseek(file, 0, SEEK_END);

    buffer_size = ftell(file);

    rewind(file);

    buffer = (char*)malloc(sizeof(char) * buffer_size);

    read_size = fread(buffer, sizeof(char), buffer_size, file);
    buffer[read_size] = '\0';

    if (buffer_size != read_size) {
        free(buffer);
        buffer = NULL;
    }

    fclose(file);
    return buffer;
}