#include <assert.h>

#include <mrkl4c.h>

#include "unittest.h"

#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif

static mrkl4c_logger_t logger;

#define FOO_LOG(level, msg, ...) \
    MRKL4C_WRITE_ONCE_PRINTFLIKE(logger, level, FOO, msg, __VA_ARGS__)

#define FOO_LOG_START(level, msg, ...) \
    MRKL4C_WRITE_START_PRINTFLIKE(logger, level, FOO, msg, __VA_ARGS__)

#define FOO_LOG_NEXT(level, msg, fmt, ...) \
    MRKL4C_WRITE_NEXT_PRINTFLIKE(logger, level, FOO, msg, fmt, __VA_ARGS__)

#define FOO_LOG_STOP(level, msg) \
    MRKL4C_WRITE_STOP_PRINTFLIKE(logger, level, FOO, msg)

#define FOO_LERROR(msg, ...) FOO_LOG(LOG_ERR, msg, __VA_ARGS__)
#define FOO_LWARNING(msg, ...) FOO_LOG(LOG_WARNING, msg, __VA_ARGS__)
#define FOO_LINFO(msg, ...) FOO_LOG(LOG_INFO, msg, __VA_ARGS__)
#define FOO_LDEBUG(msg, ...) FOO_LOG(LOG_DEBUG, msg, __VA_ARGS__)

#define FOO_LREG(msg, level) \
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

    logger = mrkl4c_open(MRKL4C_OPEN_STDERR | MRKL4C_OPEN_SHARED);

    FOO_LREG(QWE, LOG_INFO);
    FOO_LREG(ASD, LOG_ERR);

    FOO_LERROR(QWE, 1, 2.0, "qwe123123123123123123123");

    FOO_LWARNING(QWE, 11, 22.22,
            "qweQWEQWEQWE!@#!@#!@~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

    FOO_LINFO(QWE, 11, 22.22, "qweQWEQWEQWE!@#!@#!@");

    /* complex */
    FOO_LOG_START(LOG_DEBUG, ASD, "asdASDASDASDASD");
    for (i = 0; i < 12; ++i) {
        FOO_LOG_NEXT(LOG_DEBUG, ASD, " %d", i);
    }
    FOO_LOG_STOP(LOG_DEBUG, ASD);


    (void)mrkl4c_close(logger);
    mrkl4c_fini();
}

int
main(void)
{
    test0();
    return 0;
}
