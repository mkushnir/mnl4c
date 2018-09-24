#include <assert.h>
#include <time.h>

#include <mrkcommon/dumpm.h>
#include <mrkl4c.h>

#include "unittest.h"
#include "my-logdef.h"

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

static mnbytes_t _FOO = BYTES_INITIALIZER("FOO");
static int _my_number = 1;
static mnbytes_t _lz = BYTES_INITIALIZER("L0");

#define LZERO_FOO_LERROR(msg, ...) FOO_CONTEXT_LERROR(logger0, FRED("%d %s: "), msg, _my_number, BDATA(&_lz), ##__VA_ARGS__)
#define LZERO_FOO_LINFO(msg, ...) FOO_CONTEXT_LINFO(logger0, FGREEN("%d %s: "), msg, _my_number, BDATA(&_lz), ##__VA_ARGS__)
#define LZERO_TD_LDEBUG(msg, ...) TD_CONTEXT_LDEBUG(logger1, FBLUE("%d %s: "), msg, _my_number, BDATA(&_lz), ##__VA_ARGS__)

static void
test0(void)
{
    UNUSED int res;
    mrkl4c_logger_t logger0;
    mrkl4c_logger_t logger1;
    struct {
        long rnd;
        int in;
        int expected;
    } data[] = {
        {0, 0, 0},
    };
    UNITTEST_PROLOG_RAND;

    FOREACHDATA {
        //TRACE("in=%d expected=%d", CDATA.in, CDATA.expected);
        assert(CDATA.in == CDATA.expected);
    }

    mrkl4c_init();

    logger0 = mrkl4c_open(MRKL4C_OPEN_STDERR);
    assert(logger0 != -1);
    foo_init_logdef(logger0);
    logger1 = mrkl4c_open(MRKL4C_OPEN_FILE, "/tmp/mrkl4c-testfoo.log", 4096, 20.0, 10, 0);
    assert(logger1 != -1);
    (void)mrkl4c_set_bufsz(logger1, 256);
    foo_init_logdef(logger1);

    FOO_LERROR(logger0, QWE, 1, 2.0, "qwe123123123123123123123");
    FOO_LERROR(logger1, QWE, 1, 2.0, "qwe123123123123123123123");
    FOO_LINFO(logger0, ZXC);
    FOO_LINFO(logger1, ZXC);
    LZERO_FOO_LERROR(QWE, 100, 200.0, "QWE12300");
    LZERO_FOO_LINFO(ZXC);

    FOO_LWARNING(logger0, QWE1, 11, 22.22,
            "qweQWEQWEQWE!@#!@#!@~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

    FOO_LWARNING(logger1, QWE1, 11, 22.22,
            "qweQWEQWEQWE!@#!@#!@~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

    FOO_LINFO(logger0, QWE, 11, 22.22, "qweQWEQWEQWE!@#!@#!@");
    FOO_LINFO(logger1, QWE, 11, 22.22, "qweQWEQWEQWE!@#!@#!@");

    /* complex */
    res = mrkl4c_set_level(logger1, LOG_DEBUG, &_FOO);
    FOO_LOG_START(logger0, LOG_DEBUG, ASD, "start:");
    for (i = 0; i < 12; ++i) {
        FOO_LOG_NEXT(logger0, LOG_DEBUG, ASD, " %d", i);
    }
    FOO_LOG_STOP(logger0, LOG_DEBUG, ASD, " Stop.");

    FOO_LOG_START(logger1, LOG_DEBUG, ASD1, "start:");
    for (i = 0; i < 12; ++i) {
        FOO_LOG_NEXT(logger1, LOG_DEBUG, ASD1, " %d", i);
    }
    FOO_LOG_STOP(logger1, LOG_DEBUG, ASD1, " Stop.");

    BAR_LOG_START(logger1, LOG_DEBUG, ASD1, "start:");
    for (i = 0; i < 12; ++i) {
        BAR_LOG_NEXT(logger1, LOG_DEBUG, ASD1, " %d", i);
    }
    BAR_LOG_STOP(logger1, LOG_DEBUG, ASD1, " Stop.");

    for (i = 0; i < 120; ++i) {
        TD_LINFO(logger0, WER, i);
        LZERO_TD_LDEBUG(WER, i);
        sleep(1);
    }

    (void)mrkl4c_close(logger0);
    (void)mrkl4c_close(logger1);
    mrkl4c_fini();
}

int
main(void)
{
    test0();
    return 0;
}
