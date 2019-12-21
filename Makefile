# Makefile

CC = gcc

FLAGS = -o file_system *.c -std=c99 -Os -Iinclude

all: build run

build:
	$(CC) $(FLAGS)

run:
	./file_system