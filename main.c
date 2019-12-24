// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_system.h"

static void print_help(FILE* out);
static void use_menu(int argc, char** argv, FILE* output);

int main(int argc, char** argv) {
    if (fs_init(1024 * 8) != 0) {
        fprintf(stderr, "Failed to initialize file system\n");
        return -1;
    }

    if (argc > 1) {
        use_menu(argc, argv, stdout);
    }
    else {
        fs_create_dir("projects");

        {
            FSFILE* file = fs_open("debug.log", "w");
            const char* str = "This is a test\n";
            fs_write(str, strlen(str), file);
            fs_close(file);
        }

        fs_remove_file("debug.log");

        fs_list(stdout);
    }

    fs_dump_disk("./data/test.disk");

    fs_free();
    return 0;
}

void print_help(FILE* out) {
    fprintf(out,
        "COMMANDS:\n"
        " c <file>      # Create new file\n"
        " d <file>      # Create new directory\n"
        " w <file> \"contents\" # Open and write to file\n"
        " a <file> \"contents\" # Open and write (append) to file\n"
        " l             # List files\n"
        " h             # Print this list\n"
    );
}

void use_menu(int argc, char** argv, FILE* output) {
    if (argc < 2) {
        return;
    }

    const char* disk_path = "data/test.disk";
    fs_init_from_disk(disk_path);

    switch (*argv[1]) {
        case 'c': {
            if (argc >= 2) {
                FSFILE* file = fs_open(argv[2], "w");
                if (file) {
                    fs_print_file_info(file, output);
                    fs_close(file);
                }
            }
        }
            break;

        case 'd': {
            if (argc >= 2) {
                FSFILE* file = fs_create_dir(argv[2]);
                if (file) {
                    fs_print_file_info(file, output);
                    fs_close(file);
                }
            }
        }
            break;

        case 'w': {
            if (argc >= 3) {
                FSFILE* file = fs_open(argv[2], "w");
                if (file) {
                    unsigned long size = strlen(argv[3]);
                    fs_write(argv[3], size, file);
                    fs_close(file);
                }
            }
        }
            break;

        case 'a': {
            if (argc >= 3) {
                FSFILE* file = fs_open(argv[2], "a");
                if (file) {
                    unsigned long size = strlen(argv[3]);
                    fs_write(argv[3], size, file);
                    fs_close(file);
                }
            }
        }
            break;

        case 'l': {
            fs_list(output);
        }
            break;

        case 'h':
            print_help(stdout);
            break;

        default:
            break;
    }

}