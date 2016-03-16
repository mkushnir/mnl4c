#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>

#include <mrkcommon/array.h>
#include <mrkcommon/bytestream.h>
#include <mrkcommon/dumpm.h>
#include <mrkcommon/util.h>

#define SYSLOG_NAMES
#include <mrkl4c.h>

#include "diag.h"


#define MRKL4C_DEFAULT_BUFSZ 4096

static array_t ctxes;

double
mrkl4c_now_posix(void){
    struct timeval tv;

    (void)gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec) / 1000000.0;
}


static void
mrkl4c_write_stdout(mrkl4c_ctx_t *ctx)
{
    fprintf(stdout, "%s", SDATA(&ctx->bs, 0));
    bytestream_rewind(&ctx->bs);
}


static void
mrkl4c_write_stderr(mrkl4c_ctx_t *ctx)
{
    fprintf(stderr, "%s", SDATA(&ctx->bs, 0));
    bytestream_rewind(&ctx->bs);
}


static void
mrkl4c_write_file(mrkl4c_ctx_t *ctx)
{
    bytestream_rewind(&ctx->bs);
}


static void
writer_init(mrkl4c_writer_t *writer)
{
    writer->write = NULL;
    writer->data.file.path = NULL;
    writer->data.file.cursz = 0;
    writer->data.file.maxsz = 0;
    writer->data.file.maxtm = 0.0;
    writer->data.file.starttm = 0.0;
    writer->data.file.curtm = 0.0;
    writer->data.file.maxfiles = 0;
}


static void
cache_init(mrkl4c_cache_t *cache)
{
    cache->pid = getpid();
}


static void
writer_fini(mrkl4c_writer_t *writer)
{
    BYTES_DECREF(&writer->data.file.path);
}


static mrkl4c_ctx_t *
mrkl4c_ctx_new(ssize_t bsbufsz)
{
    mrkl4c_ctx_t *res;

    if ((res = malloc(sizeof(mrkl4c_ctx_t))) == NULL) {
        FAIL("malloc");
    }
    res->nref = 0;
    res->bsbufsz = bsbufsz;
    bytestream_init(&res->bs, bsbufsz);
    writer_init(&res->writer);
    cache_init(&res->cache);
    array_init(&res->minfos, sizeof(mrkl4c_minfo_t), 0, NULL, NULL);
    res->ty = 0;
    return res;
}


static void
mrkl4c_ctx_destroy(mrkl4c_ctx_t **pctx)
{
    if (*pctx != NULL) {
        bytestream_fini(&(*pctx)->bs);
        writer_fini(&(*pctx)->writer);
        free(*pctx);
        *pctx = NULL;
    }
}


int
mrkl4c_ctx_allowed(mrkl4c_ctx_t *ctx, int level, int id)
{
    mrkl4c_minfo_t *minfo;

    assert(id >= 0 && id < MRKL4C_MAX_MINFOS);
    if ((minfo = array_get(&ctx->minfos, id)) == NULL) {
        FAIL("array_get");
    }
    assert(minfo->id == id);
    assert(level >= 0 && (size_t)level < countof(level_names));
    return minfo->level <= level;
}


int
mrkl4c_set_bufsz(mrkl4c_logger_t ld, ssize_t sz)
{
    mrkl4c_ctx_t **pctx;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        return -1;
    }
    bytestream_fini(&(*pctx)->bs);
    bytestream_init(&(*pctx)->bs, sz);
    return 0;
}


void
mrkl4c_register_msg(mrkl4c_logger_t ld, int id, int level)
{
    mrkl4c_ctx_t **pctx;
    mrkl4c_minfo_t *minfo;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        FAIL("array_get");
    }
    assert(id >= 0 && id < MRKL4C_MAX_MINFOS);
    if ((minfo = array_get_safe(&(*pctx)->minfos, id)) == NULL) {
        FAIL("array_get_safe");
    }
    minfo->id = id;
    minfo->level = level;
}


mrkl4c_logger_t
mrkl4c_open(unsigned ty, ...)
{
    va_list ap;
    const char *fpath;
    size_t maxsz;
    double maxtm;
    size_t maxfiles;
    UNUSED int flags;
    mrkl4c_ctx_t **pctx;
    array_iter_t it;

    fpath = NULL;
    va_start(ap, ty);
    switch (ty & MRKL4C_OPEN_MASK) {
    case MRKL4C_OPEN_STDOUT:
    case MRKL4C_OPEN_STDERR:
        break;

    case MRKL4C_OPEN_FILE:
        fpath = va_arg(ap, const char *);
        maxsz = va_arg(ap, size_t);
        maxtm = va_arg(ap, double);
        maxfiles = va_arg(ap, size_t);
        flags = va_arg(ap, int);
        break;

    default:
        FAIL("mrkl4c_open");
        break;
    }
    va_end(ap);

    if (ty & MRKL4C_OPEN_SHARED) {
        for (pctx = array_first(&ctxes, &it);
             pctx != NULL;
             pctx = array_next(&ctxes, &it)) {
            if (*pctx != NULL) {
                if ((*pctx)->ty == ty) {
                    if (fpath != NULL) {
                        if (strcmp(fpath,
                                   (char *)(*pctx)->
                                    writer.data.file.path->data) == 0) {
                            break;
                        } else {
                            /* continue */
                        }
                    } else {
                        /* assume STDOUT/STDERR */
                        break;
                    }
                } else {
                    /* move on */
                }
            }
        }
    } else {
        pctx = NULL;
    }

    if (pctx == NULL) {
        /* first find a free slot */
        for (pctx = array_first(&ctxes, &it);
             pctx != NULL;
             pctx = array_next(&ctxes, &it)) {
            if (*pctx == NULL) {
                (void)array_init_item(&ctxes, it.iter);
                break;
            }
        }

        /* no free slot? */
        if (pctx == NULL) {
            if ((pctx = array_incr_iter(&ctxes, &it)) == NULL) {
                FAIL("array_incr_iter");
            }
            (*pctx)->ty = ty & MRKL4C_OPEN_MASK;
        }

        switch (ty & MRKL4C_OPEN_MASK) {
        case MRKL4C_OPEN_STDOUT:
            (*pctx)->writer.write = mrkl4c_write_stdout;
            break;

        case MRKL4C_OPEN_STDERR:
            (*pctx)->writer.write = mrkl4c_write_stderr;
            break;

        case MRKL4C_OPEN_FILE:
            assert(fpath != NULL);
            (*pctx)->writer.write = mrkl4c_write_file;
            (*pctx)->writer.data.file.path = bytes_new_from_str(fpath);
            (*pctx)->writer.data.file.maxsz = maxsz;
            (*pctx)->writer.data.file.maxtm = maxtm;
            (*pctx)->writer.data.file.starttm = mrkl4c_now_posix();
            (*pctx)->writer.data.file.curtm =
                (*pctx)->writer.data.file.starttm;
            (*pctx)->writer.data.file.maxfiles = maxfiles;
            break;

        default:
            FAIL("mrkl4c_open");
            break;
        }
    }

    ++(*pctx)->nref;
    return (mrkl4c_logger_t)it.iter;
}


mrkl4c_ctx_t *
mrkl4c_get_ctx(mrkl4c_logger_t ld)
{
    mrkl4c_ctx_t **pctx;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        return NULL;
    }

    return *pctx;
}


mrkl4c_logger_t
mrkl4c_incref(mrkl4c_logger_t ld)
{
    mrkl4c_logger_t res;
    mrkl4c_ctx_t **pctx;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        res = -1;
        goto end;
    }
    if (*pctx == NULL) {
        res = -1;
        goto end;
    }
    res = ld;
    (*pctx)->nref++;

end:
    return res;
}


int
mrkl4c_close(mrkl4c_logger_t ld)
{
    mrkl4c_ctx_t **pctx;
    int res;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        res = -1;
        goto end;
    }
    if (*pctx == NULL) {
        res = -1;
        goto end;
    }
    res = 0;
    (*pctx)->nref--;
    if ((*pctx)->nref <= 0) {
        array_clear_item(&ctxes, ld);
    }

end:
    return res;
}


static int
ctx_init(mrkl4c_ctx_t **ctx)
{
    assert(ctx != NULL);
    *ctx = mrkl4c_ctx_new(MRKL4C_DEFAULT_BUFSZ);
    return 0;
}


static int
ctx_fini(mrkl4c_ctx_t **ctx)
{
    assert(ctx != NULL);
    mrkl4c_ctx_destroy(ctx);
    return 0;
}


void
mrkl4c_init(void)
{
    array_init(&ctxes,
               sizeof(mrkl4c_ctx_t *),
               0,
               (array_initializer_t)ctx_init,
               (array_finalizer_t)ctx_fini);
}


void
mrkl4c_fini(void)
{
    array_fini(&ctxes);
}
