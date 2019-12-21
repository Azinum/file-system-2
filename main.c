// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_system.h"

int main(int argc, char** argv) {
    if (fs_init(1024 * 8) != 0) {
        fprintf(stderr, "Failed to initialize file system\n");
        return -1;
    }

    FSFILE* file = fs_open("test.txt", "w");

    if (file) {
        const char* string = "This is a small file. END\n";
        fs_write(string, strlen(string), file);
        fs_read(file, stdout);
        fs_close(file);
    }

    fs_dump_disk("./data/test.disk");

    fs_free();
    return 0;
}