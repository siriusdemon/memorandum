CFLAGS=-std=c11 -g -fno-common

manda: main.o
	$(CC) -o manda main.o $(LDFLAGS)

test: manda
	./test.sh

clean:
	rm -f manda *.o *~ tmp*

.PHONY: test clean
