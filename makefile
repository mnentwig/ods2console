all: ods2csv.exe
ods2csv.exe: main.cpp minizip/ioapi.c minizip/unzip.c
	g++ -g -Wall -Wextra -pedantic -O -o ods2csv.exe main.cpp minizip/ioapi.c minizip/unzip.c -lz
test: ods2csv.exe
	./ods2csv.exe sampleInput.ods
clean: 
	rm -f main.exe
.PHONY: test clean
