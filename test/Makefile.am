CLEANFILES = $(BUILT_SOURCES) *.core core
#CLEANFILES += *.in
AM_MAKEFLAGS = -s

# consider headers:
#   - include_HEADERS
#   - nobase_include_HEADERS
#   - pkginclude_HEADERS
#   - dist_HEADERS
#   - nodist_HEADERS
#   - noinst_HEADERS

noinst_PROGRAMS=testfoo testperf

diags = ../src/diag.txt diag.txt
BUILT_SOURCES = diag.c diag.h my-logdef.c my-logdef.h
EXTRA_DIST = diag.txt logdef.txt

noinst_HEADERS = unittest.h ../src/mrkl4c.h

DEBUG_LD_FLAGS =
if DEBUG
DEBUG_CC_FLAGS = -g -O0 @CC_DEBUG@
DEBUG_LD_FLAGS += -no-install
else
DEBUG_CC_FLAGS = -DNDEBUG -O3 -fomit-frame-pointer
if LTO
DEBUG_CC_FLAGS += @CC_LTO@
DEBUG_LD_FLAGS += @LD_LTO@
endif
endif

if ALLSTATIC
testfoo_LDFLAGS = -all-static
testperf_LDFLAGS = -all-static
else
testfoo_LDFLAGS =
testperf_LDFLAGS =
endif

nodist_testfoo_SOURCES = diag.c my-logdef.c
testfoo_SOURCES = testfoo.c
if LTO
testfoo_SOURCES += ../src/mrkl4c.c
endif
testfoo_CFLAGS = $(DEBUG_CC_FLAGS) -Wall -Wextra -Werror -std=c99 @_GNU_SOURCE_MACRO@ @_XOPEN_SOURCE_MACRO@ -I$(top_srcdir)/test -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)
testfoo_LDFLAGS += -L$(libdir) -L$(top_srcdir)/src/.libs
testfoo_LDADD = -lmrkl4c -lmrkcommon

nodist_testperf_SOURCES = diag.c my-logdef.c
testperf_SOURCES = testperf.c
if LTO
testperf_SOURCES += ../src/mrkl4c.c
endif
testperf_CFLAGS = $(DEBUG_CC_FLAGS) -Wall -Wextra -Werror -std=c99 @_GNU_SOURCE_MACRO@ @_XOPEN_SOURCE_MACRO@ -I$(top_srcdir)/test -I$(top_srcdir)/src -I$(top_srcdir) -I$(includedir)
testperf_LDFLAGS += -L$(libdir) -L$(top_srcdir)/src/.libs
testperf_LDADD = -lmrkl4c -lmrkcommon

diag.c diag.h: $(diags)
	$(AM_V_GEN) cat $(diags) | sort -u >diag.txt.tmp && mndiagen -v -S diag.txt.tmp -L mrkl4c -H diag.h -C diag.c ../src/*.[ch] ./*.[ch]

my-logdef.c my-logdef.h: logdef.txt
	$(AM_V_GEN) ../src/l4cdefgen --lib foo --hout my-logdef.h --cout my-logdef.c logdef.txt

testrun: all
	for i in $(noinst_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;
