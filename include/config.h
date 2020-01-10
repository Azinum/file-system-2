// config.h

#ifndef _CONFIG_H
#define _CONFIG_H

#if LOCAL_BUILD
#define DATA_PATH "."
#else
#define DATA_PATH "/usr/local/share/fs2"
#endif


#define COLORING 1

#if COLORING

#define RED "\e[1;31m"
#define BLUE "\e[1;34m"
#define DARK_GREEN "\e[0;32m"
#define NONE "\e[0m"

#else

#define RED ""
#define BLUE ""
#define DARK_GREEN ""
#define NONE ""

#endif	// COLORING

#endif