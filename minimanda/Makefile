CFLAGS=-std=c11 -g -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

manda: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): manda.h

test: manda
	./test.sh
	./test-driver.sh

clean:
	rm -f manda *.o *~ tmp*

.PHONY: test clean