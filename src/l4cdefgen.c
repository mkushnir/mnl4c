#if __STDC_VERSION__ < 201212
#   ifndef _WITH_GETLINE
#       define _WITH_GETLINE
#   endif
#endif
#include <assert.h>
#include <err.h>
#include <ctype.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mncommon/array.h>
#include <mncommon/hash.h>
#include <mncommon/bytes.h>
#include <mncommon/util.h>

#include "config.h"

#ifdef HAVE_MALLOC_H
#   include <malloc.h>
#endif

#define FAIL(s) do {perror(s); abort(); } while (0)

typedef struct _l4cgen_module {
    mnbytes_t *mid;
    mnbytes_t *name;
    mnarray_t messages;
} l4cgen_module_t;

typedef struct _l4cgen_message {
    mnbytes_t *level;
    mnbytes_t *mid;
    mnbytes_t *value;
} l4cgen_message_t;

static mnhash_t modules;


#ifndef NDEBUG
const char *_malloc_options = "AJ";
#endif


static struct option optinfo[] = {
#define L4CDEFGEN_OPT_HELP      0
    {"help", no_argument, NULL, 'h'},
#define L4CDEFGEN_OPT_VERSION   1
    {"version", no_argument, NULL, 'V'},
#define L4CDEFGEN_OPT_COUT      2
    {"cout", required_argument, NULL, 'C'},
#define L4CDEFGEN_OPT_HOUT      3
    {"hout", required_argument, NULL, 'H'},
#define L4CDEFGEN_OPT_LIB       4
    {"lib", required_argument, NULL, 'L'},
#define L4CDEFGEN_OPT_VERBOSE    5
    {"verbose", no_argument, NULL, 'v'},
};


static int verbose;
static char *cout;
static char *hout;
static char *lib;

static void
usage(char *p)
{
    printf("Usage: %s OPTIONS\n"
"\n"
"Options:\n"
"  --help|-h                    Show this message and exit.\n"
"  --version|-V                 Print version and exit.\n"
"  --lib=NAME|-LNAME            Library name. Required.\n"
"  --hout=PATH|-HPATH           Output header. Default <libname>-logdef.h.\n"
"  --cout=PATH|-CPATH           Output source. Default <libname>-logdef.c.\n"
"  --verbose|-v                 Increase verbosity.\n"
,
        basename(p));
}


static void
macroname_translate(mnbytes_t *s)
{
    size_t i;

    for (i = 0; i < (BSZ(s) - 1); ++i) {
        char c = *(BCDATA(s) + i);
        if (isalpha(c)) {
            if (islower(c)) {
                c = toupper(c);
            }
        } else {
            c = '_';
        }
        *(BDATA(s) + i) = c;
    }
    s->hash = 0ul;
}


static void
render_head(FILE *fhout, FILE *fcout, const char *hout, const char *lib)
{
    mnbytes_t *hout_macroname;

    hout_macroname = bytes_new_from_str(hout);

    macroname_translate(hout_macroname);
    fprintf(fcout, "#include <mnl4c.h>\n");
    fprintf(fcout, "#include \"%s\"\n", hout);
    fprintf(fcout,
        "void\n"
        "%s_init_logdef(mnl4c_logger_t logger)\n"
        "{\n",
        lib);

    fprintf(fhout,
        "#ifndef %s\n"
        "#define %s\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n",
        BDATA(hout_macroname),
        BDATA(hout_macroname));
    BYTES_DECREF(&hout_macroname);

}


static int
l4cgen_message_init(void *o)
{
    l4cgen_message_t *msg = o;
    msg->level = NULL;
    msg->mid = NULL;
    msg->value = NULL;
    return 0;
}


static int
l4cgen_message_fini(void *o)
{
    l4cgen_message_t *msg = o;
    BYTES_DECREF(&msg->level);
    BYTES_DECREF(&msg->mid);
    BYTES_DECREF(&msg->value);
    return 0;
}


static int
l4cgen_module_init(l4cgen_module_t *mod)
{
    mod->mid = NULL;
    mod->name = NULL;
    if (array_init(&mod->messages, sizeof(l4cgen_message_t), 0,
            l4cgen_message_init,
            l4cgen_message_fini) != 0) {
        FAIL("array_init");
    }
    return 0;
}


static int
l4cgen_module_fini(l4cgen_module_t *mod)
{
    BYTES_DECREF(&mod->mid);
    BYTES_DECREF(&mod->name);
    (void)array_fini(&mod->messages);
    return 0;
}


static l4cgen_module_t *
l4cgen_module_new(void)
{
    l4cgen_module_t *res;
    if (MNUNLIKELY((res = malloc(sizeof(l4cgen_module_t))) == NULL)) {
        FAIL("malloc");
    }
    (void)l4cgen_module_init(res);
    return res;
}

static void
l4cgen_module_destroy(l4cgen_module_t **pmod)
{
    if (*pmod != NULL) {
        (void)l4cgen_module_fini(*pmod);
        free(*pmod);
        *pmod = NULL;
    }
}


static int
l4cgen_module_fini_item(void *k, UNUSED void *value)
{
    l4cgen_module_t *key = k;
    l4cgen_module_destroy(&key);
    return 0;
}


static uint64_t
l4cgen_module_hash(void const *o)
{
    l4cgen_module_t const *mod = o;
    //assert(mod != NULL);
    //assert(mod->mid != NULL);
    return bytes_hash(mod->mid);
}


static int
l4cgen_module_cmp(void const *oa, void const *ob)
{
    l4cgen_module_t const *a = oa, *b = ob;
    //assert(a != NULL);
    //assert(b != NULL);
    //assert(a->mid != NULL);
    //assert(b->mid != NULL);
    return bytes_cmp(a->mid, b->mid);
}


#define PROCESS_LOGDEF_STATE_MODULE 0
#define PROCESS_LOGDEF_STATE_MESSAGE 1
#define PROCESS_LOGDEF_STATE_STR(st)                                           \
(                                                                              \
 (st) == PROCESS_LOGDEF_STATE_MODULE ? "PROCESS_LOGDEF_STATE_MODULE" :         \
 (st) == PROCESS_LOGDEF_STATE_MESSAGE ? "PROCESS_LOGDEF_STATE_MESSAGE" :       \
 "<unknown>"                                                                   \
)                                                                              \


static void
process_logdef(const char *fname)
{
    FILE *fp;
    char *line;
    size_t linesz;
    ssize_t nread;
    int state = PROCESS_LOGDEF_STATE_MODULE;
    l4cgen_module_t *current_mod, probe_mod;
    UNUSED l4cgen_message_t *current_msg;
    mnhash_item_t *hit;

    if ((fp = fopen(fname, "r")) == NULL) {
        if (verbose > 0) {
            fprintf(stderr, "cannot open %s, ignoring ...\n", fname);
        }
        goto end;
    }

    line = NULL;
    linesz = 0;
    current_mod = NULL;
    current_msg = NULL;

    while ((nread = getline(&line, &linesz, fp)) > 0) {
        UNUSED char *a, *b, *c;

        /* clear eol symbol */
        if (line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }
        if (verbose > 2) {
            fprintf(stderr, "linesz=%zd\n", linesz);
            fprintf(stderr, "line=%s\n", line);
        }

        a = line;
        while (*a == ' ') {
            ++a;
        }
        if ((b = strchr(a, ' ')) == NULL) {
            goto miss2;
        }
        *b++ = '\0';

        while (*b == ' ') {
            ++b;
        }

        if (*b == '\0') {
            goto miss2;
        }

        /* optional third atom */
        if ((c = strchr(b, ' ')) == NULL) {
            goto miss3;

        } else {
            *c++ = '\0';

            while (*c == ' ') {
                ++c;
            }
            if (*c == '\0') {
                goto miss3;
            }
        }

        /* message line */
        assert(a != NULL);
        assert(b != NULL);
        assert(c != NULL);
        if (verbose > 2) {
            fprintf(stderr, "a=%s\n", a);
            fprintf(stderr, "b=%s\n", b);
            fprintf(stderr, "c=%s\n", c);
        }
        if (*a == '#') {
            /* comment */
            continue;
        }
        if (current_mod == NULL) {
            if (verbose) {
                fprintf(stderr, "No module context, ignoring line: %s\n", line);
            }
        } else {
            l4cgen_message_t *msg;
            if (MNUNLIKELY((msg = array_incr(&current_mod->messages)) == NULL)) {
                FAIL("array_incr");
            }
            msg->level = bytes_new_from_str(a);
            msg->mid = bytes_new_from_str(b);
            msg->value = bytes_new_from_str(c);
        }
        continue;
miss2:
        /* missing second atom */
        if (verbose) {
            fprintf(stderr, "skipping invalid line: \"%s\"\n", line);
        }
        continue;

miss3:
        /* module section */
        assert(a != NULL);
        assert(b != NULL);
        if (verbose > 2) {
            fprintf(stderr, "a=%s\n", a);
            fprintf(stderr, "b=%s\n", b);
        }
        if (*a == '#') {
            /* comment */
            continue;
        }

        if (state == PROCESS_LOGDEF_STATE_MODULE) {
            /* previous module section was empty*/
        } else if (state == PROCESS_LOGDEF_STATE_MESSAGE) {
            state = PROCESS_LOGDEF_STATE_MODULE;
        } else {
            FAIL("process_logdef");
        }
        probe_mod.mid = bytes_new_from_str(a);
        if ((hit = hash_get_item(&modules, &probe_mod)) == NULL) {
            current_mod = l4cgen_module_new();
            current_mod->mid = probe_mod.mid;
            current_mod->name = bytes_new_from_str(b);
            hash_set_item(&modules, current_mod, NULL);
        } else {
            BYTES_DECREF(&probe_mod.mid);
            current_mod = hit->key;
        }
        continue;
    }

    if (line != NULL) {
        free(line);
    }

    fclose(fp);
end:
    return;
}



static int
mycb2(void *o, void *udata)
{
    l4cgen_message_t *msg = o;
    struct {
        FILE *fhout;
        FILE *fcout;
        const char *lib;
        l4cgen_module_t *mod;
        int idx;
    } *params = udata;

    if (verbose > 2) {
        printf("  %s: %s %s\n",
               BDATASAFE(msg->level),
               BDATASAFE(msg->mid),
               BDATASAFE(msg->value));
    }

    fprintf(params->fhout,
        "#define %s_%s_ID %d\n"
        "#define %s_%s_FMT %s\n",
        BDATA(params->mod->mid),
        BDATA(msg->mid),
        params->idx,
        BDATA(params->mod->mid),
        BDATA(msg->mid),
        BDATA(msg->value));

    fprintf(params->fcout,
        "    %s_LREG(logger, %s, %s);\n",
        BDATA(params->mod->mid),
        BDATA(msg->level),
        BDATA(msg->mid));

    ++params->idx;

    return 0;
}

static int
mycb1(void *key, UNUSED void *value, void *udata)
{
    l4cgen_module_t *mod = key;
    struct {
        FILE *fhout;
        FILE *fcout;
        const char *lib;
        l4cgen_module_t *mod;
        int idx;
    } *params = udata;

    //assert(mod->mid != NULL);
    //assert(mod->name != NULL);

    params->mod = mod;

    if (verbose > 2) {
        printf("mod %s, %s:\n", BDATASAFE(mod->mid), BDATASAFE(mod->name));
    }

    fprintf(params->fhout,
        "#define %s_LLOG(logger, msg, ...) MNL4C_WRITE_MAYBE_PRINTFLIKE_FLEVEL(logger, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LLOG(logger, context, msg, ...) MNL4C_WRITE_MAYBE_PRINTFLIKE_CONTEXT_FLEVEL(logger, context, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG(logger, level, msg, ...) MNL4C_WRITE_MAYBE_PRINTFLIKE(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LOG(logger, level, context, msg, ...) MNL4C_WRITE_MAYBE_PRINTFLIKE_CONTEXT(logger, level, context, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_LT(logger, level, msg, ...) MNL4C_WRITE_ONCE_PRINTFLIKE_LT(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_LT2(logger, level, msg, ...) MNL4C_WRITE_ONCE_PRINTFLIKE_LT2(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LOG_LT(logger, level, context, msg, ...) MNL4C_WRITE_ONCE_PRINTFLIKE_LT_CONTEXT(logger, level, context, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LOG_LT2(logger, level, context, msg, ...) MNL4C_WRITE_ONCE_PRINTFLIKE_LT2_CONTEXT(logger, level, context, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_START(logger, level, msg, ...) MNL4C_WRITE_START_PRINTFLIKE(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_CONTEXT_START(logger, level, context, msg, ...) MNL4C_WRITE_START_PRINTFLIKE_CONTEXT(logger, level, context, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_START_LT(logger, level, msg, ...) MNL4C_WRITE_START_PRINTFLIKE_LT(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_START_LT2(logger, level, msg, ...) MNL4C_WRITE_START_PRINTFLIKE_LT2(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_CONTEXT_START_LT(logger, level, context, msg, ...) MNL4C_WRITE_START_PRINTFLIKE_LT_CONTEXT(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_CONTEXT_START_LT2(logger, level, context, msg, ...) MNL4C_WRITE_START_PRINTFLIKE_LT2_CONTEXT(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_NEXT(logger, level, msg, fmt, ...) MNL4C_WRITE_NEXT_PRINTFLIKE(logger, level, %s, msg, fmt, ##__VA_ARGS__)\n"
        "#define %s_LOG_CONTEXT_NEXT(logger, level, context, msg, fmt, ...) MNL4C_WRITE_NEXT_PRINTFLIKE_CONTEXT(logger, level, context, %s, msg, fmt, ##__VA_ARGS__)\n"
        "#define %s_LOG_STOP(logger, level, msg, ...) MNL4C_WRITE_STOP_PRINTFLIKE(logger, level, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_LOG_CONTEXT_STOP(logger, level, context, msg, ...) MNL4C_WRITE_STOP_PRINTFLIKE_CONTEXT(logger, level, context, %s, msg, ##__VA_ARGS__)\n"
        "#define %s_DO_AT(logger, level, msg, __a1) MNL4C_DO_AT(logger, level, %s, msg, __a1)\n"
        "#define %s_LERROR(logger, msg, ...) %s_LOG_LT(logger, LOG_ERR, msg, ##__VA_ARGS__)\n"
        "#define %s_LERROR2(logger, msg, ...) %s_LOG_LT2(logger, LOG_ERR, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LERROR(logger, context, msg, ...) %s_CONTEXT_LOG_LT(logger, LOG_ERR, context, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LERROR2(logger, context, msg, ...) %s_CONTEXT_LOG_LT2(logger, LOG_ERR, context, msg, ##__VA_ARGS__)\n"
        "#define %s_LWARNING(logger, msg, ...) %s_LOG_LT(logger, LOG_WARNING, msg, ##__VA_ARGS__)\n"
        "#define %s_LWARNING2(logger, msg, ...) %s_LOG_LT2(logger, LOG_WARNING, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LWARNING(logger, context, msg, ...) %s_CONTEXT_LOG_LT(logger, LOG_WARNING, context, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LWARNING2(logger, context, msg, ...) %s_CONTEXT_LOG_LT2(logger, LOG_WARNING, context, msg, ##__VA_ARGS__)\n"
        "#define %s_LINFO(logger, msg, ...) %s_LOG_LT(logger, LOG_INFO, msg, ##__VA_ARGS__)\n"
        "#define %s_LINFO2(logger, msg, ...) %s_LOG_LT2(logger, LOG_INFO, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LINFO(logger, context, msg, ...) %s_CONTEXT_LOG_LT(logger, LOG_INFO, context, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LINFO2(logger, context, msg, ...) %s_CONTEXT_LOG_LT2(logger, LOG_INFO, context, msg, ##__VA_ARGS__)\n"
        "#define %s_LDEBUG(logger, msg, ...) %s_LOG(logger, LOG_DEBUG, msg, ##__VA_ARGS__)\n"
        "#define %s_CONTEXT_LDEBUG(logger, context, msg, ...) %s_CONTEXT_LOG(logger, LOG_DEBUG, context, msg, ##__VA_ARGS__)\n"
        "#define %s_LREG(logger, level, msg) mnl4c_register_msg(logger, level, %s_ ## msg ## _ID, \"%s_\" #msg)\n"
        "#define %s_NAME %s\n"
        "#define %s_PREFIX _MNL4C_TSPIDMOD_FMT\n"
        "#define %s_ARGS _MNL4C_TSPIDMOD_ARGS(%s)\n",

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->mid),
        BDATA(mod->mid),

        BDATA(mod->name),
        BDATA(mod->mid),
        BDATA(mod->mid),
        BDATA(mod->mid));
    (void)array_traverse(&mod->messages, mycb2, udata);
    return 0;
}


static void
render_body(FILE *fhout, FILE *fcout, const char *lib)
{
    struct {
        FILE *fhout;
        FILE *fcout;
        const char *lib;
        l4cgen_module_t *mod;
        int idx;
    } params = { fhout, fcout, lib, NULL, 0 };

    (void)hash_traverse(&modules, mycb1, &params);
}


static void
render_tail(FILE *fhout, FILE *fcout, const char *lib)
{
    fprintf(fcout, "}\n");
    fprintf(fhout, "void %s_init_logdef(mnl4c_logger_t);\n", lib);
    fprintf(fhout,
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "#endif\n");
}


int
main(int argc, char *argv[static argc])
{
    int i, ch, optidx;
    FILE *fhout, *fcout;

#ifdef HAVE_MALLOC_H
#   ifndef NDEBUG
    /*
     * malloc options
     */
    if (mallopt(M_CHECK_ACTION, 1) != 1) {
        FAIL("mallopt");
    }
    if (mallopt(M_PERTURB, 0x5a) != 1) {
        FAIL("mallopt");
    }
#   endif
#endif

    while ((ch = getopt_long(argc, argv, "C:hH:L:vV", optinfo, &optidx)) != -1) {
        switch (ch) {
        case 'C':
            cout = strdup(optarg);
            break;

        case 'h':
            usage(argv[0]);
            exit(0);
            break;

        case 'H':
            hout = strdup(optarg);
            break;

        case 'L':
            lib = strdup(optarg);
            break;

        case 'v':
            verbose++;
            break;

        case 'V':
            printf("%s\n", PACKAGE_STRING);
            exit(0);
            break;

        default:
            usage(argv[0]);
            exit(1);
        }
    }

    if (lib == NULL) {
        errx(1, "--lib cannot be empty. See %s --help", basename(argv[0]));
    }

    if (cout == NULL) {
        size_t sz;

        sz = strlen(lib) + 32;
        if ((cout = malloc(sz)) == NULL) {
            FAIL("malloc");
        }
        (void)snprintf(cout, sz, "%s-logdef.c", lib);
    }

    if (hout == NULL) {
        size_t sz;

        sz = strlen(lib) + 32;
        if ((hout = malloc(sz)) == NULL) {
            FAIL("malloc");
        }
        (void)snprintf(hout, sz, "%s-logdef.h", lib);
    }

    argc -= optind;
    argv += optind;

    if ((fhout = fopen(hout, "w")) == NULL) {
        errx(1, "Cannot open %s\n", hout);
    }
    if ((fcout = fopen(cout, "w")) == NULL) {
        errx(1, "Cannot open %s\n", cout);
    }

    hash_init(&modules, 127,
        l4cgen_module_hash,
        l4cgen_module_cmp,
        l4cgen_module_fini_item);

    render_head(fhout, fcout, hout, lib);
    for (i = 0; i < argc; ++i) {
        if (verbose > 2) {
            printf("argv[%i]=%s\n", i, argv[i]);
        }
        process_logdef(argv[i]);
    }
    render_body(fhout, fcout, lib);
    render_tail(fhout, fcout, lib);
    hash_fini(&modules);
    fclose(fhout);
    fclose(fcout);

    return 0;
}
