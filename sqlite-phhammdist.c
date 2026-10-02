#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sqlite3ext.h>

SQLITE_EXTENSION_INIT1

/* Newer function flags; define as 0 so the file still builds against older
 * headers (SQLITE_DETERMINISTIC: 3.8.3, SQLITE_INNOCUOUS: 3.31.0). */
#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif
#ifndef SQLITE_INNOCUOUS
#define SQLITE_INNOCUOUS 0
#endif

/* Mark the extension entry points as exported, including when building with
 * -fvisibility=hidden. */
#if defined(_WIN32) || defined(__CYGWIN__)
#define PHHAMMDIST_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define PHHAMMDIST_EXPORT __attribute__((visibility("default")))
#else
#define PHHAMMDIST_EXPORT
#endif

/*
 * Debug output. NEVER deploy a PHHAMMDIST_DEBUG build (sqlite-phhammdist_debug.so):
 * it writes argument values to the host process's stderr on every row, which
 * leaks query data into application logs and adds I/O to every call.
 */
#ifdef PHHAMMDIST_DEBUG
#include <inttypes.h>
#include <stdio.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif

/* Number of differing bits between two 64-bit hashes (0-64). */
static int ph_hamming_distance(uint64_t hash1, uint64_t hash2){
    uint64_t x = hash1 ^ hash2;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    /* Portable SWAR popcount. */
    const uint64_t m1  = 0x5555555555555555ULL;
    const uint64_t m2  = 0x3333333333333333ULL;
    const uint64_t m4  = 0x0f0f0f0f0f0f0f0fULL;
    const uint64_t h01 = 0x0101010101010101ULL;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    return (int)((x * h01) >> 56);
#endif
}

/*
 * Parse a NUL-terminated decimal string into an unsigned 64-bit value.
 * The whole string must be decimal digits: no sign, no leading whitespace,
 * no trailing characters, and the value must fit in 64 bits.
 * Returns 1 on success, 0 on invalid input.
 */
static int parse_u64(const char *str, uint64_t *out){
    char *endptr;
    unsigned long long val;

    /* strtoull() skips whitespace and accepts (and negates) a leading '-',
     * so insist that the first character is a digit. */
    if (str[0] < '0' || str[0] > '9') {
        return 0;
    }

    errno = 0;
    val = strtoull(str, &endptr, 10);
    if (errno == ERANGE || endptr == str || *endptr != '\0') {
        return 0;
    }
#if ULLONG_MAX > UINT64_MAX
    if (val > UINT64_MAX) {
        return 0;
    }
#endif

    *out = (uint64_t)val;
    return 1;
}

/*
 * Convert one argument to an unsigned 64-bit hash.
 *   INTEGER: SQLite's signed 64-bit value, reinterpreted as unsigned.
 *   TEXT:    an unsigned decimal string (see parse_u64()).
 *   BLOB:    exactly 8 bytes, big-endian (most significant byte first).
 * Anything else is an SQLITE_MISMATCH error. On failure the error is set on the context and
 * 0 is returned; on success 1 is returned.
 */
static int arg_to_u64(sqlite3_context *context, sqlite3_value *arg, int argnum, uint64_t *out){
    char *msg;
    int errcode = SQLITE_ERROR;

    switch (sqlite3_value_type(arg)) {
    case SQLITE_INTEGER:
        /* SQLite stores signed 64-bit integers (-7726118085807353866) but pHash
         * uses unsigned ones (10720625987902197750). Converting signed to
         * unsigned is well defined in C (modulo 2^64). */
        *out = (uint64_t)sqlite3_value_int64(arg);
        return 1;

    case SQLITE_TEXT: {
        /* sqlite3_value_text() always returns a NUL-terminated UTF-8 string,
         * or NULL on out-of-memory. */
        const char *str = (const char *)sqlite3_value_text(arg);
        if (str == NULL) {
            sqlite3_result_error_nomem(context);
            return 0;
        }
        if (!parse_u64(str, out)) {
            msg = sqlite3_mprintf("phhammdist: argument %d: TEXT must be an unsigned 64-bit decimal integer, got '%.40s'", argnum, str);
            break;
        }
        return 1;
    }

    case SQLITE_BLOB: {
        const unsigned char *blob = sqlite3_value_blob(arg);
        int i;
        if (sqlite3_value_bytes(arg) != 8 || blob == NULL) {
            msg = sqlite3_mprintf("phhammdist: argument %d: BLOB must be exactly 8 bytes, got %d", argnum, sqlite3_value_bytes(arg));
            break;
        }
        *out = 0;
        for (i = 0; i < 8; i++) {
            *out = (*out << 8) | blob[i];
        }
        return 1;
    }

    default:
        msg = sqlite3_mprintf("phhammdist: argument %d must be INTEGER, TEXT or an 8-byte BLOB", argnum);
        errcode = SQLITE_MISMATCH;
        break;
    }

    if (msg == NULL) {
        sqlite3_result_error_nomem(context);
    } else {
        sqlite3_result_error(context, msg, -1);
        sqlite3_result_error_code(context, errcode);
        sqlite3_free(msg);
    }
    return 0;
}

static void phhammdist(sqlite3_context *context, int argc, sqlite3_value **argv){
    uint64_t ph1;
    uint64_t ph2;

    /* Registered with nArg = 2, so SQLite never calls this with another count. */
    assert(argc == 2);
    (void)argc;

    DBG("datatype_arg1: %d datatype_arg2: %d\n", sqlite3_value_type(argv[0]), sqlite3_value_type(argv[1]));

    /* NULL in, NULL out, like SQLite's built-in functions. */
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    if (!arg_to_u64(context, argv[0], 1, &ph1) || !arg_to_u64(context, argv[1], 2, &ph2)) {
        return;
    }

    DBG("ph_hamming_distance(ph1:[%" PRIu64 "], ph2:[%" PRIu64 "])\n", ph1, ph2);

    int p_ham_distance = ph_hamming_distance(ph1, ph2);

    DBG("p_ham_distance: %d\n", p_ham_distance);

    sqlite3_result_int(context, p_ham_distance);
}

static int phhammdist_register(sqlite3 *db){
    return sqlite3_create_function(db, "phhammdist", 2,
                                   SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS,
                                   NULL, phhammdist, NULL, NULL);
}

PHHAMMDIST_EXPORT
int sqlite3_phhammdist_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi){
    (void)pzErrMsg;
    SQLITE_EXTENSION_INIT2(pApi);
    return phhammdist_register(db);
}

/* SQLite derives the default entry point from the file name: for
 * sqlite-phhammdist.so it looks for sqlite3_sqlitephhammdist_init. Provide it
 * so the extension loads without naming the entry point explicitly. */
PHHAMMDIST_EXPORT
int sqlite3_sqlitephhammdist_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi){
    return sqlite3_phhammdist_init(db, pzErrMsg, pApi);
}

#ifdef PHHAMMDIST_DEBUG
/* Default entry point derived from sqlite-phhammdist_debug.so. */
PHHAMMDIST_EXPORT
int sqlite3_sqlitephhammdistdebug_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi){
    return sqlite3_phhammdist_init(db, pzErrMsg, pApi);
}
#endif
