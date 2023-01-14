all: main.exe
main.exe:
	g++ -g -Wall -o main.exe main.cpp minizip/ioapi.c minizip/unzip.c -lz
test: main.exe
	./main.exe
clean: 
	rm -f main.exe
.PHONY: main.exe test clean
