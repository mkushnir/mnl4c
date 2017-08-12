# consider headers:
#   - include_HEADERS
#   - nobase_include_HEADERS
#   - pkginclude_HEADERS
#   - dist_HEADERS
#   - nodist_HEADERS
#   - noinst_HEADERS

diags = diag.txt
BUILT_SOURCES = diag.c diag.h logdef.c logdef.h
EXTRA_DIST = $(diags) logdef.txt gen-logdef
CLEANFILES = $(BUILT_SOURCES) *.core core
#CLEANFILES += *.in
AM_MAKEFLAGS = -s

lib_LTLIBRARIES = libmrkl4c.la

nobase_include_HEADERS = mrkl4c.h

noinst_HEADERS =

libmrkl4c_la_SOURCES = mrkl4c.c
nodist_libmrkl4c_la_SOURCES = diag.c logdef.c

if DEBUG
DEBUG_FLAGS = -g -O0 @CC_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

libmrkl4c_la_CFLAGS = $(DEBUG_FLAGS) -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @_GNU_SOURCE_MACRO@ -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)

libmrkl4c_la_LDFLAGS = -version-info 0:0:0 -L$(libdir) -lmrkcommon -lmndiag @MRKCOMMON_LOCAL_LDFLAGS@

SUBDIRS = .

diag.c diag.h: $(diags)
	$(AM_V_GEN) cat $(diags) | sort -u >diag.txt.tmp && mndiagen -S diag.txt.tmp -L mrkl4c -H diag.h -C diag.c *.[ch]

logdef.c logdef.h: logdef.txt gen-logdef
	$(AM_V_GEN) /bin/sh ./gen-logdef <logdef.txt

run: all
	for i in $(bin_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;

testrun:
	for i in $(SUBDIRS); do if test "$$i" != "."; then cd $$i && $(MAKE) testrun && cd ..; fi; done;
