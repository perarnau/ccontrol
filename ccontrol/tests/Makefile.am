# automake config
AM_COLOR_TESTS = yes

# flags for building lib check programs
AM_CFLAGS = -I$(top_srcdir)/src/lib/ -I$(top_srcdir)/src/commons/
LDADD = ../src/lib/libccontrol.la

if TEST_VALGRIND
TESTS_ENVIRONMENT= @LIBTOOL@ --mode=execute @VALGRIND@ --tool=memcheck -q --leak-check=full
endif

# all check programs
TO_COMPILE = random
TST_SH = run_random.sh 

random_SOURCES = random.c
random_CFLAGS = $(AM_CFLAGS)
random_LDADD = $(LDADD)
check_PROGRAMS = $(TO_COMPILE)
TESTS = $(TST_SH)
