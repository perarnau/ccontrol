PAPIPATH=/usr/papi-4.1.1
CCONTROLPATH=/usr/local

CFLAGS=-O2 -DUSE_PAPI -march=native -I$(PAPIPATH)/include -I$(CCONTROL_PATH)/include
LDFLAGS= -lrt -lpapi -lccontrol -I$(PAPIPATH)/lib -I$(CCONTROL_PATH)/lib

all: stencil

stencil: stencil.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) 

clean:
	rm -f stencil *.o
