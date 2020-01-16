// config.h

#ifndef _CONFIG_H
#define _CONFIG_H

#define PROGRAM_NAME "fs2"

#define DEFAULT_DISK_PATH "data/"
#define DEFAULT_DISK_NAME "test"
#define DEFAULT_DISK_SIZE (1024 << 4)

#if LOCAL_BUILD
#define DATA_PATH "."
#else
#define DATA_PATH "/usr/local/share/" PROGRAM_NAME
#endif


#define COLORING 1

#if COLORING
	#define RED "\e[1;31m"
	#define BLUE "\e[1;34m"
	#define DARK_BLUE "\e[0;34m"
	#define DARK_GREEN "\e[0;32m"
	#define NONE "\e[0m"
	
#else
	#define RED ""
	#define BLUE ""
	#define DARK_GREEN ""
	#define NONE ""
#endif

#define COLOR_MESSAGE DARK_GREEN
#define COLOR_PATH 		BLUE
#define COLOR_FILE		RED
#define COLOR_NUMBERS DARK_BLUE

// TODO(lucas): Use this type at all places needed
#define addr_t unsigned long

#endif