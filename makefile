CC = g++
CGLAGS = -Wall -ansi -pedantic

all: lib

lib: httpcpp.o
	mkdir -p ./lib
	ar -cvq ./lib/libhttpcpp.a httpcpp.o
	rm -f httpcpp.o

install:
	cp ./lib/libhttpcpp.a /usr/local/lib
	cp ./httpcpp.h /usr/local/include

example: example.o
	mkdir -p ./bin
	$(CC) $(CFLAGS) example.o -o ./bin/example -lhttpcpp
	rm -f example.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f httpcpp.o
	rm -f example.o
	rm -f ./lib/libhttpcpp.a
	rm -f ./bin/example
