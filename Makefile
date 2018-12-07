CC = gcc
CFLAGS = -Wall -g

all: minls minget 

clean :
	rm *.o

minget: minget.o min.o min.h
	$(CC) $(CFLAGS) -o minget minget.o min.o

minls: minls.o min.o min.h
	$(CC) $(CFLAGS) -o minls minls.o min.o

minget.o: minget.c
	$(CC) $(CFLAGS) -c -o minget.o minget.c

minls.o: minls.c
	$(CC) $(CFLAGS) -c -o minls.o minls.c

min.o: min.c
	$(CC) $(CFLAGS) -c -o min.o min.c
