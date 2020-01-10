// config.c
// date: 2020-01-10
// author: lucas (azinum)

#define FORMAT_LENGTH 4

struct Config_disk {
	unsigned int disk_size;
	char flag;
	char format[FORMAT_LENGTH];
};


void initialize_config(struct Config_disk* conf);
