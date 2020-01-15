// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <unistd.h>
#include <dirent.h>

#include "fs2.h"

static char args_doc[] = "";
static char doc[] = "File System 2 (fs2) - file system emulator";

// const char* name, int key, const char* arg, int flags, const char* doc, int group
static struct argp_option options[] = {
  {"create",     'c', "file",      0,  "Create new file"},
  {"read",       'r', "file",      0,  "Read file"},
  {"create-dir", 'd', "file",      0,  "Create new directory"},
  {"remove",     'x', "file",      0,  "Remove regular file"},
  {"remove-dir", 'z', "dir",       0,  "Remove directory"},
  {"change-dir", 'v', "dir",       0,  "Change directory"},
  {"list",       'l', "file",      OPTION_ARG_OPTIONAL,  "List directory contents"},
  {"write",      'w', "file",      0,  "Write data to file"},
  {"append",     'a', "file",      0,  "Append data to file"},
  {"info",       'i', "file",      0,  "Print file info"},
  {"options",    'o', 0,           0,  "Get all options"},
  {"path",       'p', "path",      0,  "Specify data path"},
  { 0 }
};

struct Arguments {
    int silent, verbose;
    FILE* output_file;
};

static error_t parse_option(int key, char *arg, struct argp_state *state);

int main(int argc, char** argv) {
    struct argp argp = {options, parse_option, args_doc, doc};
    struct Arguments arguments = {
        .silent = 0,
        .verbose = 1,
        .output_file = stdout
    };

    char* disk_path = DATA_PATH "/data/test.disk";

    if (argc > 1) {
        fs_init_from_disk(disk_path);
        if (fs_get_error() != 0) return -1;
        argp_parse(&argp, argc, argv, 0, 0, &arguments);
        fs_dump_disk(disk_path);
    }
    else {
        fs_init(DEFAULT_DISK_SIZE); // Create an empty disk
        if (fs_get_error() != 0) return -1;
        fs_dump_disk(disk_path);
    }
    return 0;
}

error_t parse_option(int key, char* arg, struct argp_state* state) {
    struct Arguments* arguments = state->input;
    int arg_count = state->argc - state->next;
    char** args = (state->argv + state->next);

    switch (key) {
        case 'c': {
            FSFILE* file = fs_open(arg, "w");
            if (fs_get_error() != 0) break;
            fs_close(file);
        }
            break;
        
        case 'r': {
            FSFILE* file = fs_open(arg, "r");
            if (fs_get_error() != 0) break;
            fs_read(file, arguments->output_file);
            fs_close(file);
            fs_get_error();
        }
            break;

        case 'd': {
            FSFILE* file = fs_create_dir(arg);
            if (fs_get_error() != 0) break;
            fs_close(file);
        }
            break;

        case 'x': {
            if (fs_remove_file(arg) != 0) {
                fs_get_error();
            }
        }
            break;

        case 'z': {
            if (fs_remove_dir(arg) != 0) {
                fs_get_error();
            }
        }
            break;

        case 'v': {
            if (fs_change_dir(arg) != 0) {
                fs_get_error();
            }
        }
            break;

        case 'l': {
            if (arg_count == 0) {
                fs_list(NULL, arguments->output_file);
                break;
            }
            FSFILE* dir = fs_open_dir(*args);
            if (fs_get_error() != 0) break;
            fs_list(dir, arguments->output_file);
            fs_get_error();
            fs_close(dir);
        }
            break;

        case 'w': {
            FSFILE* file = fs_open(arg, "w");
            if (fs_get_error() != 0) break;
            if (arg_count > 0) {
                unsigned long size = strlen(args[0]);
                fs_write(args[0], size, file);
                fs_get_error(); // In case write fails
            }
            fs_close(file);
        }
            break;

        case 'a': {
            FSFILE* file = fs_open(arg, "a");
            if (fs_get_error() != 0) break;
            if (arg_count > 0) {
                unsigned long size = strlen(args[0]);
                fs_write(args[0], size, file);
                fs_get_error();
            }
            fs_close(file);
        }
            break;

        case 'i': {
            FSFILE* file = fs_open(arg, "r");
            if (fs_get_error() != 0) break;
            fs_print_file_info(file, arguments->output_file);
            fs_close(file);
        }
            break;

        case 'o': {
            for (int i = 0; i < ARRAY_SIZE(options) - 1; i++) {
                printf("--%s -%c ", options[i].name, options[i].key);
            }
            printf("--help -? ");
            printf("\n");
        }
            break;

        case 'p': {
            DIR* dir = opendir(arg);
            if (!dir) {
                fprintf(stderr, "Failed to set path ('%s')\n", arg);
                return 0;
            }
            closedir(dir);
            FILE* file = fopen(DATA_PATH "/.path", "w");
            if (!file) {
                printf("Path file doesn't exist\n");
                return 0;
            }
            fprintf(file, "%s\n", arg);
            fclose(file);
        }
            break;

        default:
            return 0;
    }
    return 0;
}