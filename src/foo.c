#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>

#include <foo.h>
//#include <mrkcommon/dumpm.h>
//#include <mrkcommon/util.h>
#ifndef UNUSED
#   define UNUSED __attribute__ ((unused))
#endif

char message[BUFLEN];

int
initialize (UNUSED int argc, UNUSED char **argv)
{
    return 0;
}


int
run (void)
{
    return 0;
}


int
terminate (void)
{
    return 0;
}

