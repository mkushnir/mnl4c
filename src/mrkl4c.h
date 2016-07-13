#ifndef MRKL4C_H_DEFINED
#define MRKL4C_H_DEFINED

#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRKL4C_LOGGER_INVALID (-1)
typedef int mrkl4c_logger_t;

struct _mrkl4c_ctx;


typedef struct _mrkl4c_minfo {
    int id;
    /*
     * LOG_*
     */
    int level;
    bytes_t *name;
} mrkl4c_minfo_t;


#define MRKL4C_FWRITER_DEFAULT_OPEN_FLAGS (O_WRONLY | O_APPEND | O_CREAT)
#define MRKL4C_FWRITER_DEFAULT_OPEN_MODE 0644
typedef struct _mrkl4c_writer {
    void (*write)(struct _mrkl4c_ctx *);
    union {
        struct {
            bytes_t *path;
            bytes_t *shadow_path;
            size_t cursz;
            size_t maxsz;
            double maxtm;
            double starttm;
            double curtm;
            size_t maxfiles;
            int fd;
            struct stat sb;
            unsigned flags;
        } file;
    } data;
} mrkl4c_writer_t;


typedef struct _mrkl4c_cache {
    pid_t pid;
} mrkl4c_cache_t;


#define MRKL4C_MAX_MINFOS 1024
typedef struct _mrkl4c_ctx {
    ssize_t nref;
    bytestream_t bs;
    ssize_t bsbufsz;
    /* strongref */
    mrkl4c_writer_t writer;
    mrkl4c_cache_t cache;
    array_t minfos;
    unsigned ty;
} mrkl4c_ctx_t;

double mrkl4c_now_posix(void);

#define MRKL4C_OPEN_STDOUT  0x0001
#define MRKL4C_OPEN_STDERR  0x0002
#define MRKL4C_OPEN_FILE    0x0003
#define MRKL4C_OPEN_TY      0x00ff
#define MRKL4C_OPEN_FLOCK   0x0100



mrkl4c_logger_t mrkl4c_open(unsigned, ...);
int mrkl4c_set_bufsz(mrkl4c_logger_t, ssize_t);
mrkl4c_logger_t mrkl4c_incref(mrkl4c_logger_t);
mrkl4c_ctx_t *mrkl4c_get_ctx(mrkl4c_logger_t);
int mrkl4c_ctx_allowed(mrkl4c_ctx_t *, int, int);
int mrkl4c_close(mrkl4c_logger_t);
void mrkl4c_register_msg(mrkl4c_logger_t, int, int, const char *);
int mrkl4c_set_level(mrkl4c_logger_t, int, bytes_t *);
void mrkl4c_init(void);
void mrkl4c_fini(void);

UNUSED static const char *level_names[] = {
    "EMERG",
    "ALERT",
    "CRIT",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
};


#define MRKL4C_WRITE_ONCE_PRINTFLIKE(ld, level, mod, msg, ...)                 \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__);                        \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
                _mrkl4c_ctx->writer.data.file.cursz += _mrkl4c_nwritten;       \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


#define MRKL4C_WRITE_ONCE_PRINTFLIKE_LT(ld, level, mod, msg, ...)              \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%s [%d] %s %s: "                    \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__);                        \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
                _mrkl4c_ctx->writer.data.file.cursz += _mrkl4c_nwritten;       \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


#define MRKL4C_WRITE_ONCE_PRINTFLIKE_LT2(ld, level, mod, msg, ...)             \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            assert(_mrkl4c_ctx->writer.write != NULL);                         \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%lf %s [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->writer.data.file.curtm, \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__);                        \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
                _mrkl4c_ctx->writer.data.file.cursz += _mrkl4c_nwritten;       \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


#define MRKL4C_WRITE_START_PRINTFLIKE(ld, level, mod, msg, ...)                \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->                        \
                                            writer.data.file.curtm,            \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__)                         \


#define MRKL4C_WRITE_START_PRINTFLIKE_LT(ld, level, mod, msg, ...)             \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%s [%d] %s %s: "                    \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__)                         \


#define MRKL4C_WRITE_START_PRINTFLIKE_LT2(ld, level, mod, msg, ...)            \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            ssize_t _mrkl4c_nwritten;                                          \
            struct tm *_mrkl4c_tm;                                             \
            time_t _mtkl4c_now;                                                \
            char _mrkl4c_now_str[32];                                          \
            _mrkl4c_ctx->writer.data.file.curtm = mrkl4c_now_posix();          \
            _mtkl4c_now = (time_t)_mrkl4c_ctx->writer.data.file.curtm;         \
            _mrkl4c_tm = localtime(&_mtkl4c_now);                              \
            (void)strftime(_mrkl4c_now_str,                                    \
                           sizeof(_mrkl4c_now_str),                            \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mrkl4c_tm);                                        \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,            \
                                          _mrkl4c_ctx->bsbufsz,                \
                                          "%lf %s [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mrkl4c_ctx->writer.data.file.curtm, \
                                          _mrkl4c_now_str,                     \
                                          _mrkl4c_ctx->cache.pid,              \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__)                         \


#define MRKL4C_WRITE_NEXT_PRINTFLIKE(ld, level, mod, msg, fmt, ...)    \
            _mrkl4c_nwritten = bytestream_nprintf(&_mrkl4c_ctx->bs,    \
                                     _mrkl4c_ctx->bsbufsz,             \
                                     fmt,                              \
                                     __VA_ARGS__)                      \


#define MRKL4C_WRITE_STOP_PRINTFLIKE(ld, level, mod, msg)                      \
            if (_mrkl4c_nwritten < 0) {                                        \
                bytestream_rewind(&_mrkl4c_ctx->bs);                           \
            } else {                                                           \
                SADVANCEPOS(&_mrkl4c_ctx->bs, -1);                             \
                (void)bytestream_nprintf(&_mrkl4c_ctx->bs,                     \
                                         _mrkl4c_ctx->bsbufsz,                 \
                                         mod ## _ ## msg ## _FMT);             \
                (void)bytestream_cat(&_mrkl4c_ctx->bs, 1, "\n");               \
                _mrkl4c_ctx->writer.write(_mrkl4c_ctx);                        \
                _mrkl4c_ctx->writer.data.file.cursz += _mrkl4c_nwritten;       \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \



#define MRKL4C_ALLOWED_AT(ld, level, mod, msg, __a1)                           \
    do {                                                                       \
        mrkl4c_ctx_t *_mrkl4c_ctx;                                             \
        _mrkl4c_ctx = mrkl4c_get_ctx(ld);                                      \
        assert(_mrkl4c_ctx != NULL);                                           \
        if (mrkl4c_ctx_allowed(_mrkl4c_ctx, level, mod ## _ ## msg ## _ID)) {  \
            __a1                                                               \
        }                                                                      \
    } while (0)                                                                \


#ifdef __cplusplus
}
#endif
#endif /* MRKL4C_H_DEFINED */
