CC = g++
objects = filesystem.o ui.o fsutils.o user.o
out = main
default: all

run: all
	./main

all: $(objects)
	$(CC) -g -o $(out) $(objects)

ui.o: ui.cpp ui.h 
	$(CC) -g -c ui.cpp

user.o: user.c user.h
	$(CC) -g -c user.c

filesystem.o: filesystem.c filesystem.h user_struct.h
	$(CC) -g -c filesystem.c

fsutils.o: fsutils.c filesystem.h
	$(CC) -g -c fsutils.c

clean :
	rm $(objects) $(out)