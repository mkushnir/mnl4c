#ifndef MRKL4C_H_DEFINED
#define MRKL4C_H_DEFINED

#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytes.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int mrkl4c_logger_t;

struct _mrkl4c_ctx;


typedef struct _mrkl4c_minfo {
    int id;
    /*
     * LOG_*
     */
    int level;
} mrkl4c_minfo_t;


typedef struct _mrkl4c_writer {
    void (*write)(struct _mrkl4c_ctx *);
    union {
        struct {
            bytes_t *path;
            size_t cursz;
            size_t maxsz;
            double maxtm;
            double starttm;
            double curtm;
            size_t maxfiles;
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
#define MRKL4C_OPEN_MASK    0x00ff
#define MRKL4C_OPEN_SHARED  0x0100



mrkl4c_logger_t mrkl4c_open(unsigned, ...);
int mrkl4c_set_bufsz(mrkl4c_logger_t, ssize_t);
mrkl4c_logger_t mrkl4c_incref(mrkl4c_logger_t);
mrkl4c_ctx_t *mrkl4c_get_ctx(mrkl4c_logger_t);
int mrkl4c_ctx_allowed(mrkl4c_ctx_t *, int, int);
int mrkl4c_close(mrkl4c_logger_t);
void mrkl4c_register_msg(mrkl4c_logger_t, int, int);
void mrkl4c_init(void);
void mrkl4c_fini(void);

static const char *level_names[] = {
    "EMERG",
    "ALERT",
    "CRIT",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
};

#define _MRKL4C_CAT(a, b) a ## b

#define _MRKL4C_ARGS(prefix) _MRKL4C_CAT(prefix, _ARGS)

#define MRKL4C_WRITE_ONCE_PRINTFLIKE(ld, level, mod, msg, ...)                 \
    do {                                                                       \
        mrkl4c_ctx_t *ctx;                                                     \
        ssize_t nwritten;                                                      \
        ctx = mrkl4c_get_ctx(ld);                                              \
        assert(ctx != NULL);                                                   \
        if (mrkl4c_ctx_allowed(ctx, level, mod ## _ ## msg ## _ID)) {          \
            assert((ctx)->writer.write != NULL);                               \
            (ctx)->writer.data.file.curtm = mrkl4c_now_posix();                \
            nwritten = bytestream_nprintf(&(ctx)->bs,                          \
                                          (ctx)->bsbufsz,                      \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          (ctx)->writer.data.file.curtm,       \
                                          ctx->cache.pid,                      \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__);                        \
            if (nwritten < 0) {                                                \
                bytestream_rewind(&(ctx)->bs);                                 \
            } else {                                                           \
                (void)bytestream_cat(&(ctx)->bs, 2, "\n");                     \
                (ctx)->writer.write(ctx);                                      \
                (ctx)->writer.data.file.cursz += nwritten + 1;                 \
            }                                                                  \
        }                                                                      \
    } while (0)                                                                \


#define MRKL4C_WRITE_START_PRINTFLIKE(ld, level, mod, msg, ...)                \
    do {                                                                       \
        mrkl4c_ctx_t *ctx;                                                     \
        ssize_t nwritten;                                                      \
        ctx = mrkl4c_get_ctx(ld);                                              \
        assert(ctx != NULL);                                                   \
        if (mrkl4c_ctx_allowed(ctx, level, mod ## _ ## msg ## _ID)) {          \
            (ctx)->writer.data.file.curtm = mrkl4c_now_posix();                \
            nwritten = bytestream_nprintf(&(ctx)->bs,                          \
                                          (ctx)->bsbufsz,                      \
                                          "%.06lf [%d] %s %s: "                \
                                          mod ## _ ## msg ## _FMT,             \
                                          (ctx)->writer.data.file.curtm,       \
                                          ctx->cache.pid,                      \
                                          mod ## _NAME,                        \
                                          level_names[level],                  \
                                          __VA_ARGS__)                         \


#define MRKL4C_WRITE_NEXT_PRINTFLIKE(ld, level, mod, msg, fmt, ...)    \
            nwritten = bytestream_nprintf(&(ctx)->bs,                  \
                                     (ctx)->bsbufsz,                   \
                                     fmt,                              \
                                     __VA_ARGS__)                      \


#define MRKL4C_WRITE_STOP_PRINTFLIKE(ld, level, mod, msg)      \
            if (nwritten < 0) {                                \
                bytestream_rewind(&(ctx)->bs);                 \
            } else {                                           \
                (void)bytestream_cat(&(ctx)->bs, 2, "\n");     \
                (ctx)->writer.write(ctx);                      \
                (ctx)->writer.data.file.cursz += nwritten + 1; \
            }                                                  \
        }                                                      \
    } while (0)                                                \



#ifdef __cplusplus
}
#endif
#endif /* MRKL4C_H_DEFINED */
