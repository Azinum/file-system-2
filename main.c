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
        fs_test();
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
        " x <file>      # Delete file"
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
    fs_init_from_disk(disk_path);

    switch (*argv[1]) {
        case 'c': {
            if (argc > 2) {
                FSFILE* file = fs_open(argv[2], "w");
                if (!file) {
                    printf("'%s': Failed to create file\n", argv[2]);
                    break;
                }
                fs_close(file);
            }
        }
            break;

        case 'r': {
            if (argc > 2) {
                FSFILE* file = fs_open(argv[2], "r");
                if (!file) {
                    printf("'%s': Failed to read file\n", argv[2]);
                    break;
                }
                fs_read(file, output);
                fs_close(file);
            }
        }
            break;

        case 'd': {
            if (argc > 2) {
                FSFILE* file = fs_create_dir(argv[2]);
                if (!file) {
                    printf("'%s': Failed to create directory\n", argv[2]);
                    break;
                }
                fs_close(file);
            }
        }
            break;

        case 'w': {
            if (argc > 3) {
                FSFILE* file = fs_open(argv[2], "w");
                if (!file) {
                    printf("'%s': Failed to write to file\n", argv[2]);
                    break;
                }
                unsigned long size = strlen(argv[3]);
                fs_write(argv[3], size, file);
                fs_close(file);
            }
        }
            break;

        case 'a': {
            if (argc > 3) {
                FSFILE* file = fs_open(argv[2], "a");
                if (!file) {
                    printf("'%s': Failed to append data to file\n", argv[2]);
                    break;
                }
                unsigned long size = strlen(argv[3]);
                fs_write(argv[3], size, file);
                fs_close(file);
            }
        }
            break;

        case 'x': {
            if (argc > 2) {
                if (fs_remove_file(argv[2]) != 0) {
                    printf("'%s': Failed to remove file\n", argv[2]);
                }
            }
        }
            break;

        case 'i': {
            if (argc > 2) {
                FSFILE* file = fs_open(argv[2], "r");
                if (!file) {
                    printf("'%s': No such file or directory\n", argv[2]);
                    break;
                }
                fs_print_file_info(file, output);
                fs_close(file);
            }
        }
            break;

        case 'l':
            fs_list(output);
            break;

        case 'h':
            print_help(stdout);
            break;

        default:
            break;
    }
    fs_dump_disk("./data/test.disk");
}
