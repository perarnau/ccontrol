AM_CFLAGS = -I$(top_srcdir)/src/lib/ -I$(top_srcdir)/src/commons/
LDADD = $(top_srcdir)/src/lib/libccontrol.la
bin_PROGRAMS = ccontrol

ccontrol_SOURCES = main.c
ccontrol_CFLAGS = $(AM_CFLAGS) -DCCONTROL_LIB_PATH="\"$(libdir)/libccontrol-malloc.so\""
ccontrol_LDADD = $(LDADD)
