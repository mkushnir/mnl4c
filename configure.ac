AC_INIT(mrkl4c, 0.1, markiyan.kushnir@gmail.com, mrkl4c, https://github.com/mkushnir)

AS_IF([test -n "$USE_GCC"],
[
    probe="gcc${USE_GCC}"
    AS_IF([which $probe >/dev/null],
    [
        CC="gcc${USE_GCC}"
        CXX="c++${USE_GCC}"
        CPP="cpp"
    ],
    [
        AS_IF([which gcc >/dev/null],
        [
            CC="gcc"
            CXX="g++"
            CPP="cpp"
        ],
        [
            echo "Cannot find gcc"
            exit 1
        ])
    ])
    AC_SUBST(CC_DEBUG, ['-ftrapv'])
],
[
    AS_IF([which clang >/dev/null],
    [
        CC="clang -Qunused-arguments -fcolor-diagnostics"
        CXX="clang++ -Qunused-arguments -fcolor-diagnostics"
        CPP="clang-cpp"
        clang_version=`clang --version | grep version | awk '{print $4;}'`
        clang_version_mjr=`echo $clang_version | awk -F. '{print $1;}'`
        clang_version_mnr=`echo $clang_version | awk -F. '{print $2;}'`
        AS_IF([test "$clang_version_mjr" = "3" -a "$clang_version_mnr" -lt 3],
            [AC_SUBST(CC_DEBUG, ['-ftrapv -fcatch-undefined-behavior'])],
            [AC_SUBST(CC_DEBUG, ['-fsanitize=undefined-trap -fsanitize-undefined-trap-on-error -fno-sanitize=alignment'])])
        AC_SUBST(CC_LTO, ['-flto'])
        AC_SUBST(LD_LTO, ['-Wl,--lto-O3'])
    ],
    [
        CC="cc"
        CXX="c++"
        CPP="cpp"
        AC_SUBST(CC_DEBUG, ['-ftrapv'])
    ])
])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AS_CASE(["$target_os"],
    [*linux*], [AC_SUBST(_GNU_SOURCE_MACRO, ['-D_GNU_SOURCE'])],
    [AC_SUBST(_GNU_SOURCE_MACRO)]
)

AS_CASE(["$target_os"],
    [*darwin*], [AC_SUBST(_XOPEN_SOURCE_MACRO, ['-D_XOPEN_SOURCE'])],
    [AC_SUBST(_XOPEN_SOURCE_MACRO)]
)

AC_CONFIG_MACRO_DIR([m4])

AC_LANG(C)

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB
AC_PROG_INSTALL

LT_INIT
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])

dnl AM_MAINTAINER_MODE

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile man/Makefile])

AC_CONFIG_HEADERS(config.h)

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_HEADERS([fcntl.h limits.h sys/time.h syslog.h])
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_CHECK_MEMBER([struct stat.st_birthtim],
                AC_DEFINE([HAVE_ST_BIRTHTIM], [1], [Define if stat.st_birthtim exists]))


AC_PRESERVE_HELP_ORDER

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [Enable debug build (default=no)]),
              [AM_CONDITIONAL([DEBUG], [enable_debug=yes])],
              [AM_CONDITIONAL([DEBUG], [test "$enable_debug" = "yes"])])

AC_ARG_ENABLE(allstatic,
              AC_HELP_STRING([--enable-allstatic],
                             [Enable all static build (default=no)]),
              [AC_MSG_NOTICE([Will build statically])],
              [AC_MSG_NOTICE([Will not build statically])])

AM_CONDITIONAL([ALLSTATIC], [test "$enable_allstatic" = "yes"])

AC_ARG_ENABLE(lto,
              AC_HELP_STRING([--enable-lto],
                             [Enable all static build (default=no)]),
              [AC_MSG_NOTICE([Will enable LTO])],
              [AC_MSG_NOTICE([Will not enable LTO])])

AM_CONDITIONAL([LTO], [test "$enable_lto" = "yes"])

AM_CONDITIONAL([LINUX], [echo $build_os | grep linux >/dev/null])
AM_CONDITIONAL([FREEBSD], [echo $build_os | grep freebsd >/dev/null])

#dnl pass LDFLAGS=-L${LOCALBASE}/lib in the environment
#AC_CHECK_LIB(mrkcommon, _fini, [], [AC_MSG_FAILURE(libmrkcommon.so is required. Please find it at git@github.com:mkushnir/mrkcommon.git)])

AC_OUTPUT
