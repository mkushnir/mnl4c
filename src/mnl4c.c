#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libgen.h> //basename
#include <limits.h> //PATH_MAX
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <mncommon/array.h>
#include <mncommon/bytestream.h>
#define TRRET_DEBUG
#include <mncommon/dumpm.h>
#include <mncommon/traversedir.h>
#include <mncommon/util.h>

#define SYSLOG_NAMES
#include <mnl4c.h>

#include "diag.h"


#define MNL4C_DEFAULT_BUFSZ 4096

static mnarray_t ctxes;

double
mnl4c_now_posix(void){
    struct timeval tv;

    (void)gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec) / 1000000.0;
}


static void
mnl4c_write_stdout(mnl4c_ctx_t *ctx)
{
    (void)bytestream_cat(&ctx->bs, 1, "");
    fprintf(stdout, "%s", SDATA(&ctx->bs, 0));
    bytestream_rewind(&ctx->bs);
}


static void
mnl4c_write_stderr(mnl4c_ctx_t *ctx)
{
    (void)bytestream_cat(&ctx->bs, 1, "");
    fprintf(stderr, "%s", SDATA(&ctx->bs, 0));
    bytestream_rewind(&ctx->bs);
}


static void
writer_init(mnl4c_writer_t *writer)
{
    writer->write = NULL;
    writer->data.file.path = NULL;
    writer->data.file.shadow_path = NULL;
    writer->data.file.cursz = 0;
    writer->data.file.maxsz = 0;
    writer->data.file.maxtm = 0.0;
    writer->data.file.starttm = 0.0;
    writer->data.file.curtm = 0.0;
    writer->data.file.maxfiles = 0;
    writer->data.file.fd = -1;
    writer->data.file.flags = 0;
}


static int
_writer_file_cleanup_shadows_cb(const char *path,
                                struct dirent *de,
                                void *udata)
{
    struct {
        char pat[PATH_MAX];
        mnarray_t files;
    } *params = udata;

    if (de != NULL) {
        char *probe;

        if ((probe = path_join(path, de->d_name)) == NULL) {
            return 1;
        }
        if (fnmatch(params->pat, probe, FNM_PATHNAME | FNM_PERIOD) == 0) {
            char **p;

            if ((p = array_incr(&params->files)) == NULL) {
                FAIL("array_incr");
            }
            //TRACE("found: %s against %s", probe, params->pat);
            *p = probe;
        } else {
            //TRACE("not found: %s against %s", probe, pat);
            free(probe);
        }

    }
    return 0;
}


static int
_writer_file_cleanup_shadows_fini_item(char **s)
{
    if (*s != NULL) {
        //TRACE("freeing %s", *s);
        free(*s);
        *s = NULL;
    }
    return 0;
}


static int
_writer_file_cleanup_shadows_cmp(char **a, char **b)
{
    if (*a == NULL) {
        if (*b == NULL) {
            return 0;
        } else {
            return -1;
        }
    } else {
        if (*b == NULL) {
            return 1;
        } else {
            return strcmp(*a, *b);
        }
    }
    return 0;
}

static void
writer_file_cleanup_shadows(mnl4c_writer_t *writer)
{
    mnbytes_t *tmp;
    struct {
        char pat[PATH_MAX];
        mnarray_t files;
    } params;
    char **fname;
    mnarray_iter_t it;

    if (writer->data.file.maxfiles <= 0) {
        return;
    }

    tmp = bytes_new_from_bytes(writer->data.file.path);
    snprintf(params.pat, sizeof(params.pat), "%s.[0-9][0-9]*", BDATA(tmp));
    array_init(&params.files,
               sizeof(char *),
               0,
               NULL,
               (array_finalizer_t)_writer_file_cleanup_shadows_fini_item);
    if (traverse_dir(dirname(BCDATA(tmp)),
                     _writer_file_cleanup_shadows_cb,
                     &params) != 0) {
        TRACE("traverse_dir() failed, could not cleanup shadows");
    }

    if (ARRAY_ELNUM(&params.files) > writer->data.file.maxfiles) {
        array_sort(&params.files,
                   (array_compar_t)_writer_file_cleanup_shadows_cmp);

        for (fname = array_first(&params.files, &it);
             fname != NULL;
             fname = array_next(&params.files, &it)) {
            if (it.iter < ARRAY_ELNUM(&params.files) - writer->data.file.maxfiles) {
                //TRACE("unlink: %s", *fname);
                if (unlink(*fname) != 0) {
                    TRACE("Failed to unlink %s while cleaninng up shadwos",
                          *fname);
                }
            } else {
                //TRACE("keep: %s", *fname);
            }
        }
    }

    array_fini(&params.files);
    BYTES_DECREF(&tmp);
}


static int
writer_file_new_shadow(mnl4c_writer_t *writer)
{
    int oflags;
    int fd;

    writer->data.file.starttm = mnl4c_now_posix();
    BYTES_DECREF(&writer->data.file.shadow_path);
    writer->data.file.shadow_path =
        bytes_printf("%s.%ld",
                     BDATA(writer->data.file.path),
                     (unsigned long)writer->data.file.starttm);

    oflags = MNL4C_FWRITER_DEFAULT_OPEN_FLAGS;
    if ((fd = open(BCDATA(writer->data.file.shadow_path),
                     oflags,
                     MNL4C_FWRITER_DEFAULT_OPEN_MODE)) < 0) {
        TRRET(WRITER_FILE_NEW_SHADOW + 1);
    }
    if (writer->data.file.flags & MNL4C_OPEN_FLOCK) {
        if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
            TR(WRITER_FILE_NEW_SHADOW + 2);
        }
    }
    (void)close(fd);
    /* write symlink */
    if (symlink(BCDATA(writer->data.file.shadow_path),
                BCDATA(writer->data.file.path)) != 0) {
        TRRET(WRITER_FILE_NEW_SHADOW + 3);
    }
    if (lstat(BCDATA(writer->data.file.shadow_path),
              &writer->data.file.sb) != 0) {
        TRRET(WRITER_FILE_NEW_SHADOW + 4);
    }
    writer->data.file.cursz = writer->data.file.sb.st_size;
#ifdef HAVE_ST_BIRTHTIM
    writer->data.file.starttm = writer->data.file.sb.st_birthtim.tv_sec;
#else
    writer->data.file.starttm = writer->data.file.sb.st_ctime;
#endif

    writer_file_cleanup_shadows(writer);

    return 0;
}


static int _writer_file_open(mnl4c_writer_t *writer)
{
    int oflags;

    oflags = MNL4C_FWRITER_DEFAULT_OPEN_FLAGS;
    if ((writer->data.file.fd =
                open(BCDATA(writer->data.file.path),
                     oflags,
                     MNL4C_FWRITER_DEFAULT_OPEN_MODE)) < 0) {
        TRRET(_WRITER_FILE_OPEN + 1);
    }
    if (writer->data.file.flags & MNL4C_OPEN_FLOCK) {
        if (flock(writer->data.file.fd, LOCK_EX|LOCK_NB) == -1) {
            close(writer->data.file.fd);
            writer->data.file.fd = -1;
            TRRET(_WRITER_FILE_OPEN + 2);
        }
    }
    return 0;
}

static int
writer_file_check_rollover(mnl4c_writer_t *writer)
{
    int res;

    //TRACE("curtm=%lf starttm=%lf maxtm=%lf, cursz=%ld maxsz=%ld",
    //      writer->data.file.curtm,
    //      writer->data.file.starttm,
    //      writer->data.file.maxtm,
    //      writer->data.file.cursz,
    //      writer->data.file.maxsz);

    res = 0;
    if (((writer->data.file.maxtm > 0.0) &&
         (writer->data.file.curtm - writer->data.file.starttm) >
        writer->data.file.maxtm) ||
        ((writer->data.file.maxsz > 0) &&
         (writer->data.file.cursz > writer->data.file.maxsz))) {

        if (writer->data.file.fd >= 0) {
            close(writer->data.file.fd);
            writer->data.file.fd = -1;
            if (unlink(BCDATA(writer->data.file.path)) != 0) {
                TRRET(WRITER_FILE_OPEN + 2);
            }
            BYTES_DECREF(&writer->data.file.shadow_path);
            if (writer_file_new_shadow(writer) != 0) {
                TRRET(WRITER_FILE_OPEN + 3);
            }
        }
    }

    if (writer->data.file.fd < 0) {
        res = _writer_file_open(writer);
    }

    return res;
}


static int
writer_file_open(mnl4c_writer_t *writer)
{
    struct stat sb;
    /*
     * not writer->data.file.path exists ?
     *  new shadow
     * not writer->data.file.path a symlink ?
     *  unlink and new shadow :
     *  read link
     * finally:
     *  rollover
     */
    memset(&sb, '\0', sizeof(struct stat));

    if (lstat(BCDATA(writer->data.file.path), &sb) != 0) {
        if (writer_file_new_shadow(writer) != 0) {
            TRRET(WRITER_FILE_OPEN + 1);
        }

        if (lstat(BCDATA(writer->data.file.path), &sb) != 0) {
            TRRET(WRITER_FILE_OPEN + 2);
        }
    }

    if (!S_ISLNK(sb.st_mode)) {
        if (unlink(BCDATA(writer->data.file.path)) != 0) {
            TRRET(WRITER_FILE_OPEN + 3);
        }
        if (writer_file_new_shadow(writer) != 0) {
            TRRET(WRITER_FILE_OPEN + 4);
        }

    } else {
        char buf[PATH_MAX];
        ssize_t nread;

        /* read symlink */
        if ((nread = readlink(BCDATA(writer->data.file.path),
                             buf,
                             sizeof(buf))) < 0) {
            TRRET(WRITER_FILE_OPEN + 5);
        }

        buf[nread] = '\0';
        BYTES_DECREF(&writer->data.file.shadow_path);
        writer->data.file.shadow_path = bytes_new_from_str(buf);

        if (lstat(BCDATA(writer->data.file.shadow_path),
            &writer->data.file.sb) != 0) {
            if (unlink(BCDATA(writer->data.file.path)) != 0) {
                TRRET(WRITER_FILE_OPEN + 6);
            }

            if (writer_file_new_shadow(writer) != 0) {
                TRRET(WRITER_FILE_OPEN + 7);
            }
        }

        writer->data.file.cursz = writer->data.file.sb.st_size;

#ifdef HAVE_ST_BIRTHTIM
        writer->data.file.starttm = writer->data.file.sb.st_birthtim.tv_sec;
#else
        writer->data.file.starttm = writer->data.file.sb.st_ctime;
#endif
    }

    /*
     * At this point, shadow_path, path, and sb are consistent.
     */
    return writer_file_check_rollover(writer);
}


static void
mnl4c_write_file(mnl4c_ctx_t *ctx)
{
    ssize_t nwritten;

    //TRACE("cursz=%ld starttm=%lf curtm=%lf",
    //      ctx->writer.data.file.cursz,
    //      ctx->writer.data.file.starttm,
    //      ctx->writer.data.file.curtm);

    //assert(ctx->writer.data.file.fd >= 0);
    if (MNUNLIKELY(
        (nwritten = write(ctx->writer.data.file.fd,
                          SDATA(&ctx->bs, 0),
                          SEOD(&ctx->bs))) <= 0)) {
        TRACE("write failed");

    } else {
        ctx->writer.data.file.cursz += nwritten;
    }

    bytestream_rewind(&ctx->bs);

    if (writer_file_check_rollover(&ctx->writer) != 0) {
        TRACE("failed to roll over");
    }
}


static void
cache_init(mnl4c_cache_t *cache)
{
    cache->pid = getpid();
}


static void
writer_fini(mnl4c_writer_t *writer)
{
    BYTES_DECREF(&writer->data.file.path);
    BYTES_DECREF(&writer->data.file.shadow_path);
}


static int
minfo_init(mnl4c_minfo_t *minfo)
{
    minfo->name = NULL;
    minfo->throttle_threshold = -1.0l;
    minfo->nthrottled = 0;
    return 0;
}


static int
minfo_fini(mnl4c_minfo_t *minfo)
{
    BYTES_DECREF(&minfo->name);
    return 0;
}


static mnl4c_ctx_t *
mnl4c_ctx_new(ssize_t bsbufsz)
{
    mnl4c_ctx_t *res;

    if ((res = malloc(sizeof(mnl4c_ctx_t))) == NULL) {
        FAIL("malloc");
    }
    res->nref = 0;
    res->bsbufsz = bsbufsz;
    bytestream_init(&res->bs, bsbufsz);
    writer_init(&res->writer);
    cache_init(&res->cache);
    array_init(&res->minfos,
               sizeof(mnl4c_minfo_t),
               0,
               (array_initializer_t)minfo_init,
               (array_finalizer_t)minfo_fini);
    res->ty = 0;
    if (MNUNLIKELY(pthread_mutex_init(&res->mtx, NULL) != 0)) {
        FFAIL("pthread_mutex_init");
    }
    return res;
}


static void
mnl4c_ctx_destroy(mnl4c_ctx_t **pctx)
{
    if (*pctx != NULL) {
        (void)pthread_mutex_destroy(&(*pctx)->mtx);
        bytestream_fini(&(*pctx)->bs);
        writer_fini(&(*pctx)->writer);
        array_fini(&(*pctx)->minfos);
        free(*pctx);
        *pctx = NULL;
    }
}


bool
mnl4c_ctx_allowed(mnl4c_ctx_t *ctx, int level, int id)
{
    mnl4c_minfo_t *minfo;

    assert(id >= 0 && id < MNL4C_MAX_MINFOS);
    if ((minfo = array_get(&ctx->minfos, id)) == NULL) {
        FAIL("array_get");
    }
    assert(minfo->id == id);
    assert(level >= 0 && (size_t)level < countof(level_names));
    return minfo->elevel >= level;
}


int
mnl4c_set_bufsz(mnl4c_logger_t ld, ssize_t sz)
{
    mnl4c_ctx_t **pctx;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        return -1;
    }
    bytestream_fini(&(*pctx)->bs);
    bytestream_init(&(*pctx)->bs, sz);
    (*pctx)->bsbufsz = sz;
    return 0;
}


void
mnl4c_register_msg(mnl4c_logger_t ld, int level, int id, const char *name)
{
    mnl4c_ctx_t **pctx;
    mnl4c_minfo_t *minfo;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        FAIL("array_get");
    }
    assert(id >= 0 && id < MNL4C_MAX_MINFOS);
    if ((minfo = array_get_safe(&(*pctx)->minfos, id)) == NULL) {
        FAIL("array_get_safe");
    }
    (void)minfo_init(minfo);
    minfo->id = id;
    minfo->flevel = level;
    minfo->elevel = level;
    minfo->name = bytes_new_from_str(name);
    BYTES_INCREF(minfo->name);
}


int
mnl4c_set_level(mnl4c_logger_t ld, int level, const mnbytes_t *prefix)
{
    mnl4c_ctx_t **pctx;
    mnl4c_minfo_t *minfo;
    mnarray_iter_t it;
    int res;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        FAIL("array_get");
    }

    res = 0;
    if (prefix == NULL) {
        for (minfo = array_first(&(*pctx)->minfos, &it);
             minfo != NULL;
             minfo = array_next(&(*pctx)->minfos, &it)) {
            minfo->elevel = level;
            ++res;
        }
    } else {
        for (minfo = array_first(&(*pctx)->minfos, &it);
             minfo != NULL;
             minfo = array_next(&(*pctx)->minfos, &it)) {
            if (bytes_startswith(minfo->name, prefix)) {
                minfo->elevel = level;
                ++res;
            }
        }
    }
    return res;
}


int
mnl4c_set_throttling(mnl4c_logger_t ld, double threshold, const mnbytes_t *prefix)
{
    mnl4c_ctx_t **pctx;
    mnl4c_minfo_t *minfo;
    mnarray_iter_t it;
    int res;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        FAIL("array_get");
    }

    res = 0;
    if (prefix == NULL) {
        for (minfo = array_first(&(*pctx)->minfos, &it);
             minfo != NULL;
             minfo = array_next(&(*pctx)->minfos, &it)) {
            minfo->throttle_threshold = threshold;
            ++res;
        }
    } else {
        for (minfo = array_first(&(*pctx)->minfos, &it);
             minfo != NULL;
             minfo = array_next(&(*pctx)->minfos, &it)) {
            if (bytes_startswith(minfo->name, prefix)) {
                minfo->throttle_threshold = threshold;
                ++res;
            }
        }
    }
    return res;
}


mnl4c_logger_t
mnl4c_open(unsigned ty, ...)
{
    va_list ap;
    const char *fpath;
    size_t maxsz;
    double maxtm;
    size_t maxfiles;
    int flags;
    mnl4c_ctx_t **pctx;
    mnarray_iter_t it;

    fpath = NULL;
    maxsz = 0;
    maxtm = 0;
    maxfiles = 0;
    flags = 0;

    if ((ty & MNL4C_OPEN_FLOCK) &&
        ((ty & MNL4C_OPEN_TY) != MNL4C_OPEN_FILE)) {
        TRACE("non-file flock is not supported");
        return -1;
    }

    va_start(ap, ty);
    switch (ty & MNL4C_OPEN_TY) {
    case MNL4C_OPEN_STDOUT:
    case MNL4C_OPEN_STDERR:
        break;

    case MNL4C_OPEN_FILE:
        fpath = va_arg(ap, const char *);
        maxsz = va_arg(ap, size_t);
        maxtm = va_arg(ap, double);
        maxfiles = va_arg(ap, size_t);
        flags = va_arg(ap, int);
        break;

    default:
        FAIL("mnl4c_open");
        break;
    }
    va_end(ap);

    for (pctx = array_first(&ctxes, &it);
         pctx != NULL;
         pctx = array_next(&ctxes, &it)) {
        if (*pctx != NULL) {
            if ((*pctx)->ty == ty) {
                if (fpath != NULL) {
                    if (strcmp(fpath,
                               BCDATA((*pctx)->
                                writer.data.file.path)) == 0) {
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
            (*pctx)->ty = ty & MNL4C_OPEN_TY;
        }

        switch (ty & MNL4C_OPEN_TY) {
        case MNL4C_OPEN_STDOUT:
            (*pctx)->writer.write = mnl4c_write_stdout;
            (*pctx)->writer.data.file.curtm = mnl4c_now_posix();
            break;

        case MNL4C_OPEN_STDERR:
            (*pctx)->writer.write = mnl4c_write_stderr;
            (*pctx)->writer.data.file.curtm = mnl4c_now_posix();
            break;

        case MNL4C_OPEN_FILE:
            assert(fpath != NULL);
            (*pctx)->writer.write = mnl4c_write_file;
            if (*fpath != '/') {
                TRACE("fpath is not an absolute path: %s", fpath);
                goto err;
            }
            (*pctx)->writer.data.file.path = bytes_new_from_str(fpath);
            (*pctx)->writer.data.file.maxsz = maxsz;
            (*pctx)->writer.data.file.maxtm = maxtm;
            (*pctx)->writer.data.file.starttm = mnl4c_now_posix();
            (*pctx)->writer.data.file.curtm =
                (*pctx)->writer.data.file.starttm;
            (*pctx)->writer.data.file.maxfiles = maxfiles;
            (*pctx)->writer.data.file.flags = flags;
            if (writer_file_open(&(*pctx)->writer) != 0) {
                goto err;
            }
            break;

        default:
            FAIL("mnl4c_open");
            break;
        }
    }

    ++(*pctx)->nref;
    return (mnl4c_logger_t)it.iter;

err:
    (void)mnl4c_close((mnl4c_logger_t)it.iter);
    return MNL4C_LOGGER_INVALID;
}


mnl4c_ctx_t *
mnl4c_get_ctx(mnl4c_logger_t ld)
{
    mnl4c_ctx_t **pctx;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        return NULL;
    }

    return *pctx;
}


mnl4c_logger_t
mnl4c_incref(mnl4c_logger_t ld)
{
    mnl4c_logger_t res;
    mnl4c_ctx_t **pctx;

    if ((pctx = array_get(&ctxes, ld)) == NULL) {
        res = MNL4C_LOGGER_INVALID;
        goto end;
    }
    if (*pctx == NULL) {
        res = MNL4C_LOGGER_INVALID;
        goto end;
    }
    res = ld;
    ++(*pctx)->nref;

end:
    return res;
}


int
mnl4c_traverse_minfos(mnl4c_logger_t ld, array_traverser_t cb, void *udata)
{
    int res = 0;
    mnl4c_ctx_t *ctx;

    if ((ctx = mnl4c_get_ctx(ld)) == NULL) {
        res = TRAVERSE_MINFOS + 1;
        goto end;
    }

    res = array_traverse(&ctx->minfos, cb, udata);

end:
    return res;
}


int
mnl4c_close(mnl4c_logger_t ld)
{
    mnl4c_ctx_t **pctx;
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
    --(*pctx)->nref;

    if ((*pctx)->nref <= 0) {
        if (SEOD(&(*pctx)->bs) > 0) {
            assert((*pctx)->writer.write != NULL);
            (*pctx)->writer.write(*pctx);
        }
        (void)array_clear_item(&ctxes, ld);
    }

end:
    return res;
}


static int
ctx_init(mnl4c_ctx_t **ctx)
{
    assert(ctx != NULL);
    *ctx = mnl4c_ctx_new(MNL4C_DEFAULT_BUFSZ);
    return 0;
}


static int
ctx_fini(mnl4c_ctx_t **ctx)
{
    assert(ctx != NULL);
    mnl4c_ctx_destroy(ctx);
    return 0;
}


void
mnl4c_init(void)
{
    array_init(&ctxes,
               sizeof(mnl4c_ctx_t *),
               0,
               (array_initializer_t)ctx_init,
               (array_finalizer_t)ctx_fini);
}


void
mnl4c_fini(void)
{
    array_fini(&ctxes);
}
