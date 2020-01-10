# Makefile

CC=gcc

PROGRAM_NAME=fs2

FLAGS=-o $(PROGRAM_NAME) *.c -std=c99 -Iinclude -Wall

FLAGS_DEBUG=-g

FLAGS_RELEASE=-O2

INSTALL_TOP=/usr/local
INSTALL_BIN=$(INSTALL_TOP)/bin
INSTALL_SHARE=$(INSTALL_TOP)/share
DATA_PATH=$(INSTALL_SHARE)/$(PROGRAM_NAME)

all: build_local generate_sample_disk

install: build_release
	cp -a ./scripts/$(PROGRAM_NAME).bash /etc/bash_completion.d/$(PROGRAM_NAME).bash
	chmod o+x $(PROGRAM_NAME)
	cp -a ./$(PROGRAM_NAME) $(INSTALL_BIN)/$(PROGRAM_NAME)

	mkdir -p $(INSTALL_SHARE)/$(PROGRAM_NAME)
	rsync --exclude="*" -da ./log/ $(DATA_PATH)/log/	# Copy only the directory, and keep the properties
	rsync --exclude="*" -da ./data/ $(DATA_PATH)/data/
	chmod o+x $(INSTALL_SHARE)/$(PROGRAM_NAME)/

build_release:
	mkdir -p log
	$(CC) $(FLAGS) $(FLAGS_RELEASE)

build_local:
	mkdir -p log
	$(CC) $(FLAGS) $(FLAGS_RELEASE) -D LOCAL_BUILD=1

copy_local:
	cp -ar ./data/ $(INSTALL_SHARE)/$(PROGRAM_NAME)/

generate_sample_disk:
	./scripts/generate_sample_disk.bash

debug: build_debug
	lldb ./$(PROGRAM_NAME)

debug_linux: build_debug
	gdb ./$(PROGRAM_NAME)

build_debug:
	$(CC) $(FLAGS) $(FLAGS_DEBUG)
