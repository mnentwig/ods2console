all: run
run:
	g++ -Wall -o main.exe main.cpp
# extract zip file to make contents.xml visible
# UNIX: use unzip
# MINGW: use unzip-mem
# leading dash (-) suppresses command failure 
	-unzip-mem -o sampleInput.ods
	./main.exe
.PHONY: run
