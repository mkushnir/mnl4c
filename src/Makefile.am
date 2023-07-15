# consider headers:
#   - include_HEADERS
#   - nobase_include_HEADERS
#   - pkginclude_HEADERS
#   - dist_HEADERS
#   - nodist_HEADERS
#   - noinst_HEADERS

diags = diag.txt
BUILT_SOURCES = diag.c diag.h
EXTRA_DIST = $(diags)
CLEANFILES = $(BUILT_SOURCES) *.core core
#CLEANFILES += *.in
AM_MAKEFLAGS = -s
AM_LIBTOOLFLAGS = --silent

lib_LTLIBRARIES = libmnl4c.la

if DEVTOOLS
bin_PROGRAMS = l4cdefgen
endif

nobase_include_HEADERS = mnl4c.h

noinst_HEADERS =

libmnl4c_la_SOURCES = mnl4c.c
nodist_libmnl4c_la_SOURCES = diag.c

if DEVTOOLS
l4cdefgen_SOURCES = l4cdefgen.c
endif

DEBUG_LD_FLAGS =
if DEBUG
DEBUG_CC_FLAGS = -g -O0 @CC_DEBUG@
DEBUG_LD_FLAGS += @LIBTOOL_NO_INSTALL@
else
DEBUG_CC_FLAGS = -DNDEBUG -O3 -fomit-frame-pointer
if LTO
DEBUG_CC_FLAGS += @CC_LTO@
DEBUG_LD_FLAGS += @LD_LTO@
endif
endif

if ALLSTATIC
libmnl4c_la_LDFLAGS = -all-static
else
libmnl4c_la_LDFLAGS =
endif

libmnl4c_la_CFLAGS = $(DEBUG_CC_FLAGS) -Wall -Wextra -Werror -std=c99 @_GNU_SOURCE_MACRO@ @_XOPEN_SOURCE_MACRO@ -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)
libmnl4c_la_LDFLAGS += $(DEBUG_LD_FLAGS) -version-info 0:0:0 -L$(libdir)
libmnl4c_la_LIBADD = -lmncommon -lpthread

if DEVTOOLS
l4cdefgen_CFLAGS = $(DEBUG_FLAGS) -Wall -Wextra -Werror -std=c99 @_GNU_SOURCE_MACRO@ @_XOPEN_SOURCE_MACRO@ -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)
l4cdefgen_LDFLAGS = -L$(libdir)
l4cdefgen_LDADD = -lmncommon -lpthread
endif

SUBDIRS = .

diag.c diag.h: $(diags)
	$(AM_V_GEN) cat $(diags) | sort -u >diag.txt.tmp && mndiagen -v -S diag.txt.tmp -L mnl4c -H diag.h -C diag.c *.[ch]

run: all
	for i in $(bin_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;

testrun:
	for i in $(SUBDIRS); do if test "$$i" != "."; then cd $$i && $(MAKE) testrun && cd ..; fi; done;
