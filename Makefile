# Makefile

CC=gcc

FLAGS=-o file_system *.c -std=c99 -Iinclude -Wall

FLAGS_DEBUG=-g

FLAGS_RELEASE=-O2

all: build_release clear run

build_release:
	$(CC) $(FLAGS) $(FLAGS_RELEASE)

run:
	./file_system

clear:
	clear

debug: build_debug
	lldb ./file_system

debug_linux: build_debug
	gdb ./file_system

build_debug:
	$(CC) $(FLAGS) $(FLAGS_DEBUG)
