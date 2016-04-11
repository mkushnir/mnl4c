CLEANFILES = *.core core
#CLEANFILES += *.in
AM_MAKEFLAGS = -s

distdir = $(top_srcdir)/$(PACKAGE)-$(VERSION)/test
# consider headers:
#   - include_HEADERS
#   - nobase_include_HEADERS
#   - pkginclude_HEADERS
#   - dist_HEADERS
#   - nodist_HEADERS
#   - noinst_HEADERS
dist_HEADERS =

noinst_PROGRAMS=testfoo

BUILT_SOURCES = diag.c diag.h
EXTRA_DIST = diag.txt gen-diag

noinst_HEADERS = unittest.h ../src/mrkl4c.h

if DEBUG
DEBUG_FLAGS = -g -O0 @CC_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

nodist_testfoo_SOURCES = diag.c
testfoo_SOURCES = testfoo.c
testfoo_CFLAGS = $(DEBUG_FLAGS) -Wall -Wextra -Werror -std=c99 @_GNU_SOURCE_MACRO@ -I$(top_srcdir)/test -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)
testfoo_LDFLAGS = -L$(libdir) -lmrkcommon -L$(top_srcdir)/src/.libs -lmrkl4c

diag.c diag.h: diag.txt
	$(AM_V_GEN) /bin/sh ./gen-diag mrkl4c

testrun: all
	for i in $(noinst_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;
