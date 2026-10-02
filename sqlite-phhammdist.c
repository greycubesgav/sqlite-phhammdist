#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sqlite3ext.h>

SQLITE_EXTENSION_INIT1

/* SQLITE_INNOCUOUS was added in SQLite 3.31.0; build against older headers too. */
#ifndef SQLITE_INNOCUOUS
#define SQLITE_INNOCUOUS 0
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

static void phhammdist(sqlite3_context *context, int argc, sqlite3_value **argv){
    uint64_t ph1;
    uint64_t ph2;

    if (argc != 2) {
        sqlite3_result_error(context, "phhammdist: requires 2 TEXT or 2 INTEGER arguments.", -1);
        return;
    }

    int datatype_arg1 = sqlite3_value_type(argv[0]);
    int datatype_arg2 = sqlite3_value_type(argv[1]);

    DBG("datatype_arg1: %d datatype_arg2: %d\n", datatype_arg1, datatype_arg2);

    if (datatype_arg1 == SQLITE_TEXT && datatype_arg2 == SQLITE_TEXT) {
        /* sqlite3_value_text() always returns a NUL-terminated UTF-8 string,
         * or NULL on out-of-memory. */
        const char *str1 = (const char *)sqlite3_value_text(argv[0]);
        const char *str2 = (const char *)sqlite3_value_text(argv[1]);

        if (str1 == NULL || str2 == NULL) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (!parse_u64(str1, &ph1) || !parse_u64(str2, &ph2)) {
            sqlite3_result_error(context, "phhammdist: TEXT arguments must be unsigned 64-bit decimal integers.", -1);
            return;
        }
    } else if (datatype_arg1 == SQLITE_INTEGER && datatype_arg2 == SQLITE_INTEGER) {

        // SQLITE stores SIGNED 64bit INTS (-7726118085807353866)
        // The Phash function uses UNSIGNED 64 bit INTS (10720625987902197750)
        // Therefore we need to convert any SIGNED ints (-7726118085807353866) into UNSIGNED ints (10720625987902197750) before checking the distance between the two

        DBG("int_ph1: %" PRId64 ", int_ph2: %" PRId64 "\n",
            (int64_t)sqlite3_value_int64(argv[0]), (int64_t)sqlite3_value_int64(argv[1]));

        ph1 = (uint64_t)sqlite3_value_int64(argv[0]);
        ph2 = (uint64_t)sqlite3_value_int64(argv[1]);
    } else {
        sqlite3_result_error(context, "phhammdist: both arguments need to be of type TEXT or both arguments need to be of type INTEGER.", -1);
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

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_phhammdist_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi){
    (void)pzErrMsg;
    SQLITE_EXTENSION_INIT2(pApi);
    return phhammdist_register(db);
}
