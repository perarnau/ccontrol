AM_CPPFLAGS = -I$(srcdir)/../commons/
lib_LTLIBRARIES = libccontrol.la libccontrol-malloc.la

libccontrol_la_SOURCES = ccontrol.c freelist.c
include_HEADERS = ccontrol.h

libccontrol_malloc_la_SOURCES = libc_bypass.c ccontrol.c freelist.c
