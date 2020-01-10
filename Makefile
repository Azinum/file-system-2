# Makefile

CC=gcc

PROGRAM_NAME=fs2

FLAGS=-o $(PROGRAM_NAME) *.c -std=c99 -Iinclude -Wall

FLAGS_DEBUG=-g

FLAGS_RELEASE=-O2

INSTALL_TOP=/usr/local
INSTALL_BIN=$(INSTALL_TOP)/bin
INSTALL_SHARE=$(INSTALL_TOP)/share

all: build_release generate_sample_disk

install: build_release generate_sample_disk
	cp -a ./scripts/$(PROGRAM_NAME).bash /etc/bash_completion.d/$(PROGRAM_NAME).bash
	chmod o+x $(PROGRAM_NAME)
	mkdir -p $(INSTALL_SHARE)/$(PROGRAM_NAME)
	mkdir -p $(INSTALL_SHARE)/$(PROGRAM_NAME)/log
	cp -ar ./data/ $(INSTALL_SHARE)/$(PROGRAM_NAME)/
	cp -a ./$(PROGRAM_NAME) $(INSTALL_BIN)/$(PROGRAM_NAME)
	chmod o+x $(INSTALL_SHARE)/$(PROGRAM_NAME)/

build_release:
	mkdir -p log
	$(CC) $(FLAGS) $(FLAGS_RELEASE)

generate_sample_disk:
	./scripts/generate_sample_disk.bash

debug: build_debug
	lldb ./$(PROGRAM_NAME)

debug_linux: build_debug
	gdb ./$(PROGRAM_NAME)

build_debug:
	$(CC) $(FLAGS) $(FLAGS_DEBUG)
