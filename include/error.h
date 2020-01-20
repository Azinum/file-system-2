// error.h

#ifndef _ERROR_H
#define _ERROR_H

#include <stdio.h>
#include <stdlib.h>

int is_error();

void error(char* format, ...);

int get_error(FILE* out);

#endif