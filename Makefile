all: test

flags = -Wall -Os

test: test.c json.h Makefile
	$(CC) $(flags) -o test test.c

run-test: test
	./test
