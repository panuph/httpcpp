CC=g++
AR=ar -cvq
COPY=cp
REMOVE=rm -f
MKDIR=mkdir -p
TAR=tar -zcvf
CGLAGS=-Wall -ansi -pedantic
ARCHIVE=httpcpp-1.0.0

all: libhttpcpp.a

libhttpcpp.a: httpcpp.o
	$(MKDIR) ./lib
	$(AR) ./lib/$@ httpcpp.o
	$(REMOVE) httpcpp.o

archive:
	$(MKDIR) ./$(ARCHIVE)
	$(COPY) httpcpp.h httpcpp.cpp example.cpp makefile README.md ./$(ARCHIVE)
	$(TAR) $(ARCHIVE).tar.gz ./$(ARCHIVE)
	$(REMOVE) -r ./$(ARCHIVE)

install:
	$(COPY) ./lib/libhttpcpp.a /usr/local/lib
	$(COPY) ./httpcpp.h /usr/local/include

example: example.o
	$(MKDIR) ./bin
	$(CC) $(CFLAGS) example.o -o ./bin/$@ -lhttpcpp
	$(REMOVE) example.o

.cpp.o:
	$(CC) $(CFLAGS) -c $<

clean:
	$(REMOVE) *.o
	$(REMOVE) ./lib/libhttpcpp.a
	$(REMOVE) ./bin/example
