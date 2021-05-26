CC = g++
objects = filesystem.o ui.o fsutils.o
out = main
default: all

run: all
	./main

all: $(objects)
	$(CC) -g -o $(out) $(objects)

ui.o: ui.cpp ui.h filesystem.h
	$(CC) -g -c ui.cpp

filesystem.o: filesystem.c filesystem.h
	$(CC) -g -c filesystem.c

fsutils.o: fsutils.c filesystem.h
	$(CC) -g -c fsutils.c

clean :
	rm $(objects) $(out)