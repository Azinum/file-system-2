// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>

#include "file_system.h"

static char args_doc[] = "";
static char doc[] = "File System 2 (fs2) - a file system emulator";

static struct argp_option options[] = {
  {"create",     'c', "file",      0,  "Create new file"},
  {"read",       'r', "file",      0,  "Read file"},
  {"create-dir", 'd', "file",      0,  "Create new directory"},
  {"remove",     'x', "file",      0,  "Remove regular file"},
  {"list",       'l', "file",      OPTION_ARG_OPTIONAL,  "List directory contents"},
  {"write",      'w', "file",      0,  "Write data to file"},
  {"append",     'a', "file",      0,  "Append data to file"},
  {"info",       'i', "file",      0,  "Print file info"},
  { 0 }
};

struct Arguments {
    char* args[2];
    int silent, verbose;
    FILE* output_file;
};

static error_t parse_option(int key, char *arg, struct argp_state *state);

int main(int argc, char** argv) {
    struct argp argp = {options, parse_option, args_doc, doc};
    struct Arguments arguments = {
        .args = {"", ""},
        .silent = 0,
        .verbose = 1,
        .output_file = stdout
    };

    char* disk_path = "./data/test.disk";

    if (argc > 1) {
        fs_init_from_disk(disk_path);
        argp_parse(&argp, argc, argv, 0, 0, &arguments);
        fs_dump_disk(disk_path);
    }
    else {
        fs_init(1024 << 4); // Create an empty disk
        fs_dump_disk(disk_path);
        (void)fs_test;
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
        default:
            return 0;
    }
    return 0;
}