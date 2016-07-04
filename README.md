Logger for Markiyan's projects
==============================

This library provides a set of macros and supporting functionality to
implement efficient and flexible logging in the C language programs.

The basic _logger_ abstraction allows to register individual log messages,
and later manage their logging level at run time.  The _gen-logdef_ script
automates generating of log message definitions and logging macros out of
user-provided plain text file of a simple format.

To start using the library, first copy the _src/gen-logdef_ to your
project, then create a _logdef.txt_ in your project, define a group of log
messages, and add messages in this format:

```text
FOO "foo"
    LOG_INFO QWE "This is the test"
```

In your Makefile.am (or other build tool):
```make
logdef.c logdef.h: logdef.txt
	$(AM_V_GEN) /bin/sh ./gen-logdef <logdef.txt
```

Check the contents of _logdef.c_ and _logdef.h_.  Add _logdef.c_ to your
project's source files, and include _logdef.h_ in your program.


Make sure the library is globally initialized and finalized in your
program by calling `mrkl4c_init()` and `mrkl4c_fini()` in appropriate
places.

Then you can use several independent loggers by managing them with
`mrkl4c_open()`,  `mrkl4c_close()`, `mrkl4c_set_bufsz()`, and
`mrkl4c_set_level()`.


You then can register individual log messages with any of the opened
loggers by calling `init_logdef()`.

Actual logging is performed by module-specific generated macro families,
for example:

```C
    FOO_LINFO(logger, QWE, NULL);
```

In this example, `FOO` is the name of the logging group, `LINFO` is
a macro class that will generate logging code at the INFO level, `logger`
is your `mrkl4c_logger_t` instance, `QWE` is the marco ID of the log
message, and `NULL` represents an empty list of printf-line arguments.

The following line would be produced:

```text
2016-07-04 17:31:18 [23743] foo INFO: This is the test
```

