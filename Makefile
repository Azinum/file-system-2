# Makefile

CC = gcc

FLAGS = -o file_system *.c -std=c99 -O2 -Iinclude

all: build run

build:
	$(CC) $(FLAGS)

run:
	./file_system