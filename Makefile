# 	Author:		Patrick Earl
#	Makefile for CSC552 Project 3

.DEFAULT_GOAL := all

# CC = /opt/gcc6/bin/g++
CC = /usr/bin/g++
FLAGS = # -g # -std=c++11
# LIBRARY = -L/opt/gcc6/lib64 -static-libstdc++

all: client server doxy

client: client.cpp
	$(CC) $(FLAGS) -c client.cpp
	$(CC) client.o $(LIBRARY) -o client

server: server.cpp
	$(CC) $(FLAGS) -c server.cpp
	$(CC) server.o $(LIBRARY) -o server

doxygen doxy:
	doxygen doxyfile

clearmem: 
	python shm_clear.py
	@echo "Cleared shared memory"

clean: clearmem
	rm -rf *.o
	rm -rf client server
	@echo "Removed executables and objects!"
