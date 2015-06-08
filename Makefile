all: main

main: main.c
	gcc -Wall -std=gnu99 main.c fat16.c -o test -g

run: main
	./test
