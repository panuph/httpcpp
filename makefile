CC = g++
CGLAGS = -Wall

lib: httpcpp.o
	ar -cvq libhttpcpp.a httpcpp.o
	rm -f httpcpp.o

example: example.o lib
	$(CC) $(CFLAGS) example.o -o example -L ./ -lhttpcpp

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f httpcpp.o
	rm -f libhttpcpp.a
	rm -f example.o
