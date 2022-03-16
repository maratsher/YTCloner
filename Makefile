CC=gcc
CFLAGS=-c -Wall
all:
	gcc cloner.c -o YTCLoner
clean:
	rm -rf *.o hello
