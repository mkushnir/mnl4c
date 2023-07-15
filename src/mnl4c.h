#ifndef MNL4C_H_DEFINED
#define MNL4C_H_DEFINED

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>


#include <mncommon/array.h>
#include <mncommon/bytes.h>
#include <mncommon/bytestream.h>
#include <mncommon/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MNL4C_LOGGER_INVALID (-1)
typedef int mnl4c_logger_t;

struct _mnl4c_ctx;


typedef struct _mnl4c_minfo {
    int id;
    /*
     * LOG_*
     */
    int flevel;
    int elevel;
    mnbytes_t *name;
    double throttle_threshold;
    int nthrottled;
} mnl4c_minfo_t;


#define MNL4C_FWRITER_DEFAULT_OPEN_FLAGS (O_WRONLY | O_APPEND | O_CREAT)
#define MNL4C_FWRITER_DEFAULT_OPEN_MODE 0644
typedef struct _mnl4c_writer {
    void (*write)(struct _mnl4c_ctx *);
    union {
        struct {
            mnbytes_t *path;
            mnbytes_t *shadow_path;
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
} mnl4c_writer_t;


typedef struct _mnl4c_cache {
    pid_t pid;
} mnl4c_cache_t;


#define MNL4C_MAX_MINFOS 1024
typedef struct _mnl4c_ctx {
    pthread_mutex_t mtx;
    ssize_t nref;
    mnbytestream_t bs;
    ssize_t bsbufsz;
    /* strongref */
    mnl4c_writer_t writer;
    mnl4c_cache_t cache;
    mnarray_t minfos;
    unsigned ty;
} mnl4c_ctx_t;

double mnl4c_now_posix(void);

#define MNL4C_OPEN_STDOUT  0x0001
#define MNL4C_OPEN_STDERR  0x0002
#define MNL4C_OPEN_FILE    0x0003
#define MNL4C_OPEN_TY      0x00ff
#define MNL4C_OPEN_FLOCK   0x0100



mnl4c_logger_t mnl4c_open(unsigned, ...);

#define MNL4C_OPEN_FROM_FILE(path, maxsz, maxtm, maxbkp, flags)\
mnl4c_open(                                                    \
    MNL4C_OPEN_FILE,                                           \
    MNTYPECHK(char *, (path)),                                 \
    MNTYPECHK(size_t, (maxsz)),                                \
    MNTYPECHK(double, (maxtm)),                                \
    MNTYPECHK(size_t, (maxbkp)),                               \
    MNTYPECHK(int, (flags)))                                   \


int mnl4c_set_bufsz(mnl4c_logger_t, ssize_t);
mnl4c_logger_t mnl4c_incref(mnl4c_logger_t);
mnl4c_ctx_t *mnl4c_get_ctx(mnl4c_logger_t);
int mnl4c_traverse_minfos(mnl4c_logger_t, array_traverser_t, void *);
bool mnl4c_ctx_allowed(mnl4c_ctx_t *, int, int);
int mnl4c_close(mnl4c_logger_t);
void mnl4c_register_msg(mnl4c_logger_t, int, int, const char *);
int mnl4c_set_level(mnl4c_logger_t, int, const mnbytes_t *);
int mnl4c_set_throttling(mnl4c_logger_t, double, const mnbytes_t *);
void mnl4c_init(void);
void mnl4c_fini(void);

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


/*
 * may be flevel
 */
#define MNL4C_WRITE_MAYBE_PRINTFLIKE_FLEVEL(ld, mod, msg, ...)         \
    do {                                                               \
        mnl4c_ctx_t *_mnl4c_ctx;                                       \
        mnl4c_minfo_t *_mnl4c_minfo;                                   \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                \
        assert(_mnl4c_ctx != NULL);                                    \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                    \
        _mnl4c_minfo = ARRAY_GET(                                      \
            mnl4c_minfo_t,                                             \
            &_mnl4c_ctx->minfos,                                       \
            mod ## _ ## msg ## _ID);                                   \
        if (mnl4c_ctx_allowed(_mnl4c_ctx,                              \
                               _mnl4c_minfo->flevel,                   \
                               mod ## _ ## msg ## _ID)) {              \
            ssize_t _mnl4c_nwritten;                                   \
            double _mnl4c_curtm;                                       \
            assert(_mnl4c_ctx->writer.write != NULL);                  \
            _mnl4c_curtm = mnl4c_now_posix();                          \
            if (_mnl4c_ctx->writer.data.file.curtm +                   \
                    _mnl4c_minfo->throttle_threshold <=                \
                    _mnl4c_curtm) {                                    \
                _mnl4c_ctx->writer.data.file.curtm =                   \
                    _mnl4c_curtm;                                      \
                _mnl4c_nwritten = bytestream_nprintf(                  \
                        &_mnl4c_ctx->bs,                               \
                        _mnl4c_ctx->bsbufsz,                           \
                        "%.06lf [%d] %s %s[%d]:\t"                     \
                        mod ## _ ## msg ## _FMT,                       \
                        _mnl4c_ctx->writer.data.file.curtm,            \
                        _mnl4c_ctx->cache.pid,                         \
                        mod ## _NAME,                                  \
                        level_names[_mnl4c_minfo->flevel],             \
                        _mnl4c_minfo->                                 \
                          nthrottled,                                  \
                        ##__VA_ARGS__);                                \
                if (_mnl4c_nwritten < 0) {                             \
                    bytestream_rewind(&_mnl4c_ctx->bs);                \
                } else {                                               \
                    SADVANCEPOS(&_mnl4c_ctx->bs, -1);                  \
                    (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");    \
                    if (SEOD(&_mnl4c_ctx->bs) >= _mnl4c_ctx->bsbufsz) {\
                        _mnl4c_ctx->writer.write(_mnl4c_ctx);          \
                    }                                                  \
                }                                                      \
                _mnl4c_minfo->nthrottled = 0;                          \
            } else {                                                   \
                ++_mnl4c_minfo->nthrottled;                            \
            }                                                          \
        }                                                              \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                        \


/*
 * may be context flevel
 */
#define MNL4C_WRITE_MAYBE_PRINTFLIKE_CONTEXT_FLEVEL(                   \
        ld, context, mod, msg, ...)                                    \
    do {                                                               \
        mnl4c_ctx_t *_mnl4c_ctx;                                       \
        mnl4c_minfo_t *_mnl4c_minfo;                                   \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                \
        assert(_mnl4c_ctx != NULL);                                    \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                    \
        _mnl4c_minfo = ARRAY_GET(                                      \
            mnl4c_minfo_t,                                             \
            &_mnl4c_ctx->minfos,                                       \
            mod ## _ ## msg ## _ID);                                   \
        if (mnl4c_ctx_allowed(_mnl4c_ctx,                              \
                               _mnl4c_minfo->flevel,                   \
                               mod ## _ ## msg ## _ID)) {              \
            ssize_t _mnl4c_nwritten;                                   \
            double _mnl4c_curtm;                                       \
            assert(_mnl4c_ctx->writer.write != NULL);                  \
            _mnl4c_curtm = mnl4c_now_posix();                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();    \
            if (_mnl4c_ctx->writer.data.file.curtm +                   \
                    _mnl4c_minfo->throttle_threshold <=                \
                    _mnl4c_curtm) {                                    \
                _mnl4c_ctx->writer.data.file.curtm =                   \
                    _mnl4c_curtm;                                      \
                _mnl4c_nwritten = bytestream_nprintf(                  \
                        &_mnl4c_ctx->bs,                               \
                        _mnl4c_ctx->bsbufsz,                           \
                        "%.06lf [%d] %s %s[%d]:\t"                     \
                        context                                        \
                        mod ## _ ## msg ## _FMT,                       \
                        _mnl4c_ctx->                                   \
                          writer.data.file.curtm,                      \
                        _mnl4c_ctx->cache.pid,                         \
                        mod ## _NAME,                                  \
                        level_names[_mnl4c_minfo->flevel],             \
                        _mnl4c_minfo->                                 \
                          nthrottled,                                  \
                        ##__VA_ARGS__);                                \
                if (_mnl4c_nwritten < 0) {                             \
                    bytestream_rewind(&_mnl4c_ctx->bs);                \
                } else {                                               \
                    SADVANCEPOS(&_mnl4c_ctx->bs, -1);                  \
                    (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");    \
                    if (SEOD(&_mnl4c_ctx->bs) >= _mnl4c_ctx->bsbufsz) {\
                        _mnl4c_ctx->writer.write(_mnl4c_ctx);          \
                    }                                                  \
                }                                                      \
                _mnl4c_minfo->nthrottled = 0;                          \
            } else {                                                   \
                ++_mnl4c_minfo->nthrottled;                            \
            }                                                          \
        }                                                              \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                        \


/*
 * may be
 */
#define MNL4C_WRITE_MAYBE_PRINTFLIKE(ld, level, mod, msg, ...)                 \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            double _mnl4c_curtm;                                               \
            mnl4c_minfo_t *_mnl4c_minfo;                                       \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_curtm = mnl4c_now_posix();                                  \
            _mnl4c_minfo = ARRAY_GET(                                          \
                mnl4c_minfo_t,                                                 \
                &_mnl4c_ctx->minfos,                                           \
                mod ## _ ## msg ## _ID);                                       \
            if (_mnl4c_ctx->writer.data.file.curtm +                           \
                    _mnl4c_minfo->throttle_threshold <= _mnl4c_curtm) {        \
                _mnl4c_ctx->writer.data.file.curtm = _mnl4c_curtm;             \
                _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,          \
                                              _mnl4c_ctx->bsbufsz,             \
                                              "%.06lf [%d] %s %s[%d]:\t"       \
                                              mod ## _ ## msg ## _FMT,         \
                                              _mnl4c_ctx->                     \
                                                writer.data.file.curtm,        \
                                              _mnl4c_ctx->cache.pid,           \
                                              mod ## _NAME,                    \
                                              level_names[level],              \
                                              _mnl4c_minfo->                   \
                                                nthrottled,                    \
                                              ##__VA_ARGS__);                  \
                if (_mnl4c_nwritten < 0) {                                     \
                    bytestream_rewind(&_mnl4c_ctx->bs);                        \
                } else {                                                       \
                    SADVANCEPOS(&_mnl4c_ctx->bs, -1);                          \
                    (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");            \
                    if (SEOD(&_mnl4c_ctx->bs) >= _mnl4c_ctx->bsbufsz) {        \
                        _mnl4c_ctx->writer.write(_mnl4c_ctx);                  \
                    }                                                          \
                }                                                              \
                _mnl4c_minfo->nthrottled = 0;                                  \
            } else {                                                           \
                ++_mnl4c_minfo->nthrottled;                                    \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * may be context
 */
#define MNL4C_WRITE_MAYBE_PRINTFLIKE_CONTEXT(                                  \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            double _mnl4c_curtm;                                               \
            mnl4c_minfo_t *_mnl4c_minfo;                                       \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_curtm = mnl4c_now_posix();                                  \
            _mnl4c_minfo = ARRAY_GET(                                          \
                mnl4c_minfo_t,                                                 \
                &_mnl4c_ctx->minfos,                                           \
                mod ## _ ## msg ## _ID);                                       \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            if (_mnl4c_ctx->writer.data.file.curtm +                           \
                    _mnl4c_minfo->throttle_threshold <= _mnl4c_curtm) {        \
                _mnl4c_ctx->writer.data.file.curtm = _mnl4c_curtm;             \
                _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,          \
                                              _mnl4c_ctx->bsbufsz,             \
                                              "%.06lf [%d] %s %s[%d]:\t"       \
                                              context                          \
                                              mod ## _ ## msg ## _FMT,         \
                                              _mnl4c_ctx->                     \
                                                writer.data.file.curtm,        \
                                              _mnl4c_ctx->cache.pid,           \
                                              mod ## _NAME,                    \
                                              level_names[level],              \
                                              _mnl4c_minfo->                   \
                                                nthrottled,                    \
                                              ##__VA_ARGS__);                  \
                if (_mnl4c_nwritten < 0) {                                     \
                    bytestream_rewind(&_mnl4c_ctx->bs);                        \
                } else {                                                       \
                    SADVANCEPOS(&_mnl4c_ctx->bs, -1);                          \
                    (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");            \
                    if (SEOD(&_mnl4c_ctx->bs) >= _mnl4c_ctx->bsbufsz) {        \
                        _mnl4c_ctx->writer.write(_mnl4c_ctx);                  \
                    }                                                          \
                }                                                              \
                _mnl4c_minfo->nthrottled = 0;                                  \
            } else {                                                           \
                ++_mnl4c_minfo->nthrottled;                                    \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once flevel
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_FLEVEL(ld, mod, msg, ...)                  \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        mnl4c_minfo_t *_mnl4c_minfo;                                           \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        _mnl4c_minfo = ARRAY_GET(                                              \
            mnl4c_minfo_t,                                                     \
            &_mnl4c_ctx->minfos,                                               \
            mod ## _ ## msg ## _ID);                                           \
        if (mnl4c_ctx_allowed(_mnl4c_ctx,                                      \
                               _mnl4c_minfo->flevel,                           \
                               mod ## _ ## msg ## _ID)) {                      \
            ssize_t _mnl4c_nwritten;                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%.06lf [%d] %s %s:\t"               \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->                         \
                                            writer.data.file.curtm,            \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[_mnl4c_minfo->flevel],   \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once context flevel
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_CONTEXT_FLEVEL(ld, context, mod, msg, ...) \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        mnl4c_minfo_t *_mnl4c_minfo;                                           \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        _mnl4c_minfo = ARRAY_GET(                                              \
            mnl4c_minfo_t,                                                     \
            &_mnl4c_ctx->minfos,                                               \
            mod ## _ ## msg ## _ID);                                           \
        if (mnl4c_ctx_allowed(_mnl4c_ctx,                                      \
                               _mnlc3_minfo->flevel,                           \
                               mod ## _ ## msg ## _ID)) {                      \
            ssize_t _mnl4c_nwritten;                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%.06lf [%d] %s %s:\t"               \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->                         \
                                            writer.data.file.curtm,            \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[_mnl4c_minfo->flevel],   \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE(ld, level, mod, msg, ...)                  \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%.06lf [%d] %s %s:\t"               \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->                         \
                                            writer.data.file.curtm,            \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once context
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_CONTEXT(ld, level, context, mod, msg, ...) \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%.06lf [%d] %s %s:\t"               \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->                         \
                                            writer.data.file.curtm,            \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once lt
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_LT(ld, level, mod, msg, ...)               \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%s [%d] %s %s:\t"                   \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once lt context
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_LT_CONTEXT(                                \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%s [%d] %s %s:\t"                   \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once lt2
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_LT2(ld, level, mod, msg, ...)              \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%lf %s [%d] %s %s:\t"               \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->writer.data.file.curtm,  \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * once lt2 context
 */
#define MNL4C_WRITE_ONCE_PRINTFLIKE_LT2_CONTEXT(                               \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            assert(_mnl4c_ctx->writer.write != NULL);                          \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%lf %s [%d] %s %s:\t"               \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->writer.data.file.curtm,  \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \
            if (_mnl4c_nwritten < 0) {                                         \
                bytestream_rewind(&_mnl4c_ctx->bs);                            \
            } else {                                                           \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                              \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");                \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                          \
            }                                                                  \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \


/*
 * start
 */
#define MNL4C_WRITE_START_PRINTFLIKE(ld, level, mod, msg, ...)                 \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%.06lf [%d] %s %s:\t"               \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->                         \
                                            writer.data.file.curtm,            \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start context
 */
#define MNL4C_WRITE_START_PRINTFLIKE_CONTEXT(                                  \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%.06lf [%d] %s %s:\t"               \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->                         \
                                            writer.data.file.curtm,            \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt
 */
#define MNL4C_WRITE_START_PRINTFLIKE_LT(ld, level, mod, msg, ...)              \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%s [%d] %s %s:\t"                   \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt context
 */
#define MNL4C_WRITE_START_PRINTFLIKE_LT_CONTEXT(                               \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%s [%d] %s %s:\t"                   \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt2
 */
#define MNL4C_WRITE_START_PRINTFLIKE_LT2(ld, level, mod, msg, ...)             \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%lf %s [%d] %s %s:\t"               \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->writer.data.file.curtm,  \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * start lt2 context
 */
#define MNL4C_WRITE_START_PRINTFLIKE_LT2_CONTEXT(                              \
        ld, level, context, mod, msg, ...)                                     \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                  \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            ssize_t _mnl4c_nwritten;                                           \
            struct tm *_mnl4c_tm;                                              \
            time_t _mtkl4c_now;                                                \
            char _mnl4c_now_str[32];                                           \
            _mnl4c_ctx->writer.data.file.curtm = mnl4c_now_posix();            \
            _mtkl4c_now = (time_t)_mnl4c_ctx->writer.data.file.curtm;          \
            _mnl4c_tm = localtime(&_mtkl4c_now);                               \
            (void)strftime(_mnl4c_now_str,                                     \
                           sizeof(_mnl4c_now_str),                             \
                           "%Y-%m-%d %H:%M:%S",                                \
                           _mnl4c_tm);                                         \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                          _mnl4c_ctx->bsbufsz,                 \
                                          "%lf %s [%d] %s %s:\t"               \
                                          context                              \
                                          mod ## _ ## msg ## _FMT,             \
                                          _mnl4c_ctx->writer.data.file.curtm,  \
                                          _mnl4c_now_str,                      \
                                          _mnl4c_ctx->cache.pid,               \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          ##__VA_ARGS__);                      \


/*
 * next
 */
#define MNL4C_WRITE_NEXT_PRINTFLIKE(ld, level, mod, msg, fmt, ...)     \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,      \
                                     _mnl4c_ctx->bsbufsz,              \
                                     fmt,                              \
                                     ##__VA_ARGS__)                    \


/*
 * next context
 */
#define MNL4C_WRITE_NEXT_PRINTFLIKE_CONTEXT(                           \
        ld, level, context, mod, msg, fmt, ...)                        \
            _mnl4c_nwritten = bytestream_nprintf(&_mnl4c_ctx->bs,      \
                                     _mnl4c_ctx->bsbufsz,              \
                                     context                           \
                                     fmt,                              \
                                     ##__VA_ARGS__)                    \


/*
 * stop
 */
#define MNL4C_WRITE_STOP_PRINTFLIKE(ld, level, mod, msg, ...)          \
            if (_mnl4c_nwritten < 0) {                                 \
                bytestream_rewind(&_mnl4c_ctx->bs);                    \
            } else {                                                   \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                      \
                (void)bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                         _mnl4c_ctx->bsbufsz,          \
                                         mod ## _ ## msg ## _FMT,      \
                                         ##__VA_ARGS__);               \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");        \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                  \
            }                                                          \
        }                                                              \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                        \



/*
 * stop context
 */
#define MNL4C_WRITE_STOP_PRINTFLIKE_CONTEXT(                           \
        ld, level, context, mod, msg, ...)                             \
            if (_mnl4c_nwritten < 0) {                                 \
                bytestream_rewind(&_mnl4c_ctx->bs);                    \
            } else {                                                   \
                SADVANCEPOS(&_mnl4c_ctx->bs, -1);                      \
                (void)bytestream_nprintf(&_mnl4c_ctx->bs,              \
                                         _mnl4c_ctx->bsbufsz,          \
                                         context                       \
                                         mod ## _ ## msg ## _FMT,      \
                                         ##__VA_ARGS__);               \
                (void)bytestream_cat(&_mnl4c_ctx->bs, 1, "\n");        \
                _mnl4c_ctx->writer.write(_mnl4c_ctx);                  \
            }                                                          \
        }                                                              \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                        \



/*
 * do at
 */
#define MNL4C_DO_AT(ld, level, mod, msg, __a1)                                 \
    do {                                                                       \
        mnl4c_ctx_t *_mnl4c_ctx;                                               \
        _mnl4c_ctx = mnl4c_get_ctx(ld);                                        \
        assert(_mnl4c_ctx != NULL);                                            \
        (void)pthread_mutex_lock(&_mnl4c_ctx->mtx);                    \
        if (mnl4c_ctx_allowed(_mnl4c_ctx, level, mod ## _ ## msg ## _ID)) {    \
            __a1                                                               \
        }                                                                      \
        (void)pthread_mutex_unlock(&_mnl4c_ctx->mtx);                  \
    } while (0)                                                                \



#ifdef __cplusplus
}
#endif
#endif /* MNL4C_H_DEFINED */
