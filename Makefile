# Makefile

CC=gcc

FLAGS=-o file_system *.c -std=c99 -Iinclude -Wall

FLAGS_DEBUG=-g

FLAGS_RELEASE=-Os # -Werror

all: build_release clear run

build_release:
	$(CC) $(FLAGS) $(FLAGS_RELEASE)

run:
	./file_system

clear:
	clear

debug: clear build_debug
	lldb ./file_system

build_debug:
	$(CC) $(FLAGS) $(FLAGS_DEBUG)