#include <assert.h>

#include <mncommon/dumpm.h>
#include <mncommon/bytes.h>
#include <mncommon/profile.h>
#include <mncommon/unittest.h>

#include <mnl4c.h>

#include "my-logdef.h"

mnl4c_logger_t logger;

#define WLEN 50
static int
randword(unsigned char *p)
{
    int i, j;

    i = (random() % (WLEN - 5)) + 3;

    for (j = 0; j < i; ++j) {
        *p = (random() % 26) + 65;
        ++p;
    }
    *p = ' ';
    assert((i + 1) < WLEN);
    return i + 1;
}


static mnbytes_t *
randline(UNUSED unsigned n)
{
    mnbytes_t *s;
    unsigned char *p;
    unsigned i = n;

    s = bytes_new((i + 1) * WLEN);
    bytes_memset(s, 0);
    p = BDATA(s);
    while (i--) {
        p += randword(p);
    }

    return s;
}


static void
dosomething(int d, float f, mnbytes_t *s)
{
    //FOO_LINFO(logger, QWE1, d, f, BDATA(s));
    FOO_LDEBUG(logger, QWE1, d, f, BDATA(s));
}

int
main(int argc, UNUSED char *argv[static argc])
{
    struct {
        int rnd;
    } data[] = {
        {0,},
    };
    UNITTEST_PROLOG_RAND;
    unsigned n;
    BYTES_ALLOCA(_foo, "FOO");

    mnl4c_init();

    logger = mnl4c_open(MNL4C_OPEN_FILE, "/tmp/mnl4c-perf.log", 1024*1024*16, 0.0, 10, 0);
    (void)mnl4c_set_bufsz(logger, 1024*1024*4);
    foo_init_logdef(logger);
    (void)mnl4c_set_level(logger, LOG_DEBUG, _foo);
    (void)mnl4c_set_throttling(logger, 0.1, _foo);

    n = 2230;
    while (n--) {
        mnbytes_t *s;

        s = randline(n);
        dosomething(n, (float)(n * 2), s);
        BYTES_DECREF(&s);
    }


    (void)mnl4c_close(logger);
    mnl4c_fini();
    return 0;
}
