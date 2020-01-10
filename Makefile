# Makefile

CC=gcc

FLAGS=-o file_system *.c -std=c99 -Iinclude -Wall

FLAGS_DEBUG=-g

FLAGS_RELEASE=-O2

all: build_release run_generate

build_release:
	@if [ ! -d "./log/" ]; then \
		mkdir log; \
	fi
	$(CC) $(FLAGS) $(FLAGS_RELEASE)

run:
	./file_system

run_generate:
	./scripts/generate/generate_sample_disk.bash

clear:
	clear

debug: build_debug
	lldb ./file_system

debug_linux: build_debug
	gdb ./file_system

build_debug:
	$(CC) $(FLAGS) $(FLAGS_DEBUG)
