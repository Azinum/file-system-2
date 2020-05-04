# Makefile

CC=gcc

PROGRAM_NAME=fs2

SRC_FILES=src/*.c

FLAGS=-o $(PROGRAM_NAME) $(SRC_FILES) -std=c99 -Iinclude -Wall -largp

# Need to link argp on OSX
FLAGS_MAC=-largp

FLAGS_DEBUG=-g

FLAGS_RELEASE=-O2

INSTALL_TOP=/usr/local
INSTALL_BIN=$(INSTALL_TOP)/bin
INSTALL_SHARE=$(INSTALL_TOP)/share
DATA_PATH=$(INSTALL_SHARE)/$(PROGRAM_NAME)

all: build_local generate_sample_disk

prepare:
	


install: build_release
	@if [ -d /etc/bash_completion.d/ ]; then \
		cp -a ./scripts/$(PROGRAM_NAME).bash /etc/bash_completion.d/$(PROGRAM_NAME).bash; \
	fi \

	chmod o+x $(PROGRAM_NAME)
	mkdir -p $(INSTALL_SHARE)/$(PROGRAM_NAME)/
	cp -a ./$(PROGRAM_NAME) $(INSTALL_BIN)/$(PROGRAM_NAME)

	chmod o+x $(INSTALL_SHARE)/$(PROGRAM_NAME)/
	rsync -a ./$(PROGRAM_NAME) $(INSTALL_SHARE)/$(PROGRAM_NAME)

	rsync --exclude="*" -da ./log/ $(DATA_PATH)/log/
	rsync --exclude="*" -da ./data/ $(DATA_PATH)/data/

uninstall: dummy
	@if [ -f "/etc/bash_completion.d/$(PROGRAM_NAME).bash" ]; then \
		rm /etc/bash_completion.d/$(PROGRAM_NAME).bash; \
	fi \
	
	@if [ -f $(INSTALL_BIN)/$(PROGRAM_NAME) ]; then \
		rm $(INSTALL_BIN)/$(PROGRAM_NAME); \
	fi \

	@if [ -d $(DATA_PATH) ]; then \
		rm -dr $(DATA_PATH); \
	fi \

	@echo "Uninstall complete!" 

build_release: prepare
	$(CC) $(FLAGS) $(FLAGS_RELEASE)

build_local: prepare
	$(CC) $(FLAGS) $(FLAGS_RELEASE) -D LOCAL_BUILD=1

copy_local:
	rsync -da ./data/ $(DATA_PATH)/data/

generate_sample_disk:
	./scripts/generate_sample_disk.bash

debug: build_debug
	lldb ./$(PROGRAM_NAME)

debug_linux: build_debug
	gdb ./$(PROGRAM_NAME)

build_debug:
	$(CC) $(FLAGS) $(FLAGS_DEBUG)

dummy:
