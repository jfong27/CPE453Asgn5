CC = gcc
CFLAGS = -Wall -g

all: minls minget

minget: minget.o
	$(CC) $(CFLAGS) -o minget minget.o

minls: minls.o
	$(CC) $(CFLAGS) -o minls minls.o

minget.o: minget.c
	$(CC) $(CFLAGS) -c -o minget.o minget.c

minls.o: minls.c
	$(CC) $(CFLAGS) -c -o minls.o minls.c