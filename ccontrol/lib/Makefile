CFLAGS ?= -O2 -g
CFLAGS += -Wall -fPIC

OBJS= ccontrol.o freelist.o

ccontrol.so: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared $^ -o $@

ccontrol.o: ccontrol.c ccontrol.h colorset.h freelist.h
	$(CC) $(CFLAGS) -o $@ -c $<

freelist.o: freelist.c freelist.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o *.so
