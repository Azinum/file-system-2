// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_system.h"

void print_help(FILE* out);
void use_menu(int argc, char** argv, FILE* output);


int main(int argc, char** argv) {
    if (argc > 1) {
        use_menu(argc, argv, stdout);
    }
    else {
        fs_init(1024 << 4);
        fs_dump_disk("./data/test.disk");
        (void)fs_test;
    }
    return 0;
}

void print_help(FILE* out) {
    fprintf(out,
        "COMMANDS:\n"
        " c <file>      # Create new file\n"
        " r <file>      # Read file\n"
        " d <file>      # Create new directory\n"
        " w <file> \"contents\" # Open and write to file\n"
        " a <file> \"contents\" # Open and write (append) to file\n"
        " x <file>      # Delete file\n"
        " i <file>      # View file info\n"
        " l             # List files\n"
        " h             # Print this list\n"
    );
}

void use_menu(int argc, char** argv, FILE* output) {
    if (argc < 2) {
        return;
    }

    const char* disk_path = "./data/test.disk";
    if (fs_init_from_disk(disk_path)) {
        fs_get_error();
        return;
    }

    switch (*argv[1]) {
        case 'c': {
            if (argc > 2) {
                FSFILE* file = fs_open(argv[2], "w");
                if (fs_get_error() != 0) break;
                fs_close(file);
            }
        }
            break;

        case 'r': {
            if (argc > 2) {
                FSFILE* file = fs_open(argv[2], "r");
                if (fs_get_error() != 0) break;
                fs_read(file, output);
                fs_close(file);
                fs_get_error();
            }
        }
            break;

        case 'd': {
            if (argc > 2) {
                FSFILE* file = fs_create_dir(argv[2]);
                if (fs_get_error() != 0) break;
                fs_close(file);
            }
        }
            break;

        case 'w': {
            if (argc > 3) {
                FSFILE* file = fs_open(argv[2], "w");
                if (fs_get_error() != 0) break;

                unsigned long size = strlen(argv[3]);
                fs_write(argv[3], size, file);
                fs_get_error(); // In case write fails
                fs_close(file);
            }
        }
            break;

        case 'a': {
            if (argc > 3) {
                FSFILE* file = fs_open(argv[2], "a");
                if (fs_get_error() != 0) break;
                unsigned long size = strlen(argv[3]);
                fs_write(argv[3], size, file);
                fs_get_error();
                fs_close(file);
            }
        }
            break;

        case 'x': {
            if (argc > 2) {
                if (fs_remove_file(argv[2]) != 0) {
                    fs_get_error();
                }
            }
        }
            break;

        case 'i': {
            if (argc > 2) {
                FSFILE* file = fs_open(argv[2], "r");
                if (fs_get_error() != 0) break;
                fs_print_file_info(file, output);
                fs_close(file);
            }
        }
            break;

        case 'l':
            if (argc > 2) {
                // TODO(lucas): Add function to open folders
                FSFILE* dir = fs_open_dir(argv[2]);
                if (fs_get_error() != 0) break;
                fs_list(dir, output);
                fs_get_error();
                fs_close(dir);
                break;
            }
            fs_list(NULL, output);
            
            break;

        case 'h':
            print_help(stdout);
            break;

        default:
            break;
    }
    fs_dump_disk("./data/test.disk");
}
