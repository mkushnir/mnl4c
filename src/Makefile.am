distdir = $(top_srcdir)/$(PACKAGE)-$(VERSION)/src
# consider headers:
#   - include_HEADERS
#   - nobase_include_HEADERS
#   - pkginclude_HEADERS
#   - dist_HEADERS
#   - nodist_HEADERS
#   - noinst_HEADERS
dist_HEADERS =

BUILT_SOURCES = diag.c diag.h logmsg.c logmsg.h
EXTRA_DIST = diag.txt gen-diag gen-logmsg
CLEANFILES = $(BUILT_SOURCES) *.core core
#CLEANFILES += *.in
AM_MAKEFLAGS = -s

lib_LTLIBRARIES = libmrkl4c.la

nobase_include_HEADERS = mrkl4c.h

noinst_HEADERS =

libmrkl4c_la_SOURCES = mrkl4c.c
nodist_libmrkl4c_la_SOURCES = diag.c logmsg.c

if DEBUG
DEBUG_FLAGS = -g -O0 @CC_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

libmrkl4c_la_CFLAGS = $(DEBUG_FLAGS) -Wall -Wextra -Werror -std=c99 @_GNU_SOURCE_MACRO@ -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)

libmrkl4c_la_LDFLAGS = -version-info 0:0:0 -L$(libdir) -lmrkcommon

SUBDIRS = .

diag.c diag.h: diag.txt
	$(AM_V_GEN) /bin/sh ./gen-diag mrkl4c

logmsg.c logmsg.h: logmsg.txt
	$(AM_V_GEN) /bin/sh ./gen-logmsg <logmsg.txt

run: all
	for i in $(bin_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;

testrun:
	for i in $(SUBDIRS); do if test "$$i" != "."; then cd $$i && $(MAKE) testrun && cd ..; fi; done;
