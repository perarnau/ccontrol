obj-m = ccontrol.o
EXTRA_CFLAGS += -I$(PWD)/../commons/
M=$(shell pwd)

all: cache_info.h module

cache_info.h:
	./detect_cache

module:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(M) modules

clean:
	rm cache_info.h
	make -C /lib/modules/$(shell uname -r)/build/ M=$(M) clean

check: all
	exit 0
