CC=gcc
CFLAGS+= -ansi -pedantic -Wall -Werror $(EXTRA_CFLAGS)

all: libmalloc.so

libmalloc.so: malloc.o
	$(CC) $(LDFLAGS) -shared $^ -o $@

malloc.o: malloc.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $^
clean:
	rm -f *.o *.so
