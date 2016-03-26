#include <assert.h>

#include <mrkcommon/dumpm.h>

#include <mrkl4c.h>

#include "unittest.h"

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

#define FOO_LOG(logger, level, msg, ...) \
    MRKL4C_WRITE_ONCE_PRINTFLIKE(logger, level, FOO, msg, __VA_ARGS__)

#define FOO_LOG_START(logger, level, msg, ...) \
    MRKL4C_WRITE_START_PRINTFLIKE(logger, level, FOO, msg, __VA_ARGS__)

#define FOO_LOG_NEXT(logger, level, msg, fmt, ...) \
    MRKL4C_WRITE_NEXT_PRINTFLIKE(logger, level, FOO, msg, fmt, __VA_ARGS__)

#define FOO_LOG_STOP(logger, level, msg) \
    MRKL4C_WRITE_STOP_PRINTFLIKE(logger, level, FOO, msg)

#define FOO_LERROR(logger, msg, ...) FOO_LOG(logger, LOG_ERR, msg, __VA_ARGS__)
#define FOO_LWARNING(logger, msg, ...) FOO_LOG(logger, LOG_WARNING, msg, __VA_ARGS__)
#define FOO_LINFO(logger, msg, ...) FOO_LOG(logger, LOG_INFO, msg, __VA_ARGS__)
#define FOO_LDEBUG(logger, msg, ...) FOO_LOG(logger, LOG_DEBUG, msg, __VA_ARGS__)

#define FOO_LREG(logger, msg, level) \
    mrkl4c_register_msg(logger, FOO_ ## msg ## _ID, level)

#define FOO_NAME "foo"
#define FOO_PREFIX _MRKL4C_TSPIDMOD_FMT
#define FOO_ARGS _MRKL4C_TSPIDMOD_ARGS(FOO)

#define FOO_QWE_ID 0
#define FOO_QWE_FMT "int %d double %lf str %s"
#define FOO_ASD_ID 1
#define FOO_ASD_FMT "%s ..."

static void
test0(void)
{
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
    logger1 = mrkl4c_open(MRKL4C_OPEN_FILE, "foo.log");

    FOO_LREG(logger0, QWE, LOG_INFO);
    FOO_LREG(logger0, ASD, LOG_ERR);

    FOO_LREG(logger1, QWE, LOG_ERR);
    FOO_LREG(logger1, ASD, LOG_ERR);

    FOO_LERROR(logger0, QWE, 1, 2.0, "qwe123123123123123123123");
    FOO_LERROR(logger1, QWE, 1, 2.0, "qwe123123123123123123123");

    FOO_LWARNING(logger0, QWE, 11, 22.22,
            "qweQWEQWEQWE!@#!@#!@~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

    FOO_LWARNING(logger1, QWE, 11, 22.22,
            "qweQWEQWEQWE!@#!@#!@~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

    FOO_LINFO(logger0, QWE, 11, 22.22, "qweQWEQWEQWE!@#!@#!@");
    FOO_LINFO(logger1, QWE, 11, 22.22, "qweQWEQWEQWE!@#!@#!@");

    /* complex */
    FOO_LOG_START(logger0, LOG_DEBUG, ASD, "asdASDASDASDASD");
    FOO_LOG_START(logger1, LOG_DEBUG, ASD, "asdASDASDASDASD");
    for (i = 0; i < 12; ++i) {
        FOO_LOG_NEXT(logger0, LOG_DEBUG, ASD, " %d", i);
        FOO_LOG_NEXT(logger1, LOG_DEBUG, ASD, " %d", i);
    }
    FOO_LOG_STOP(logger0, LOG_DEBUG, ASD);
    FOO_LOG_STOP(logger1, LOG_DEBUG, ASD);


    (void)mrkl4c_close(logger0);
    mrkl4c_fini();
}

int
main(void)
{
    test0();
    return 0;
}
