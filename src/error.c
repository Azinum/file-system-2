// error.c

#include "error.h"

#include <stdarg.h>

#define ERROR_MESSAGE_MAX_LENGTH 1024

struct Error {
	char message[ERROR_MESSAGE_MAX_LENGTH];
	int status;
};

static struct Error err = {
	.message = "",
	.status = 0
};

int is_error() {
	return err.status;
}

void error(char* format, ...) {
    va_list args;
    va_start(args, format);

    vsnprintf(err.message, ERROR_MESSAGE_MAX_LENGTH, format, args);

    va_end(args);

    err.status = 1;
}


int get_error(FILE* out) {
	if (!out)
		return 0;
	if (!is_error())
		return 0;

	fprintf(out, "%s", err.message);
	err.status = 0;
	err.message[0] = '\0';

	return 1;
}
