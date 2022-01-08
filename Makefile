all: parser

main.o: main.c
	gcc -g -ggdb -O0 -c main.c $(CFLAGS)

parser: main.o
	gcc -g -ggdb -O0 -o parser main.o $(LDFLAGS)

clean:
	rm -rf *.o
	rm -f parser