/*
 * Test driver for the phhammdist SQLite extension.
 *
 * Usage: test_phhammdist <path-to-extension>
 *
 * Loads the extension into an in-memory database, runs every case and prints
 * the results in TAP (Test Anything Protocol) format. Exits 0 if every test
 * passed, 1 otherwise.
 *
 * To add a test, add a line to the cases[] table below.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

enum expect { EXPECT_INT, EXPECT_NULL, EXPECT_ERROR };

struct test_case {
    const char *name;
    const char *sql;            /* a SELECT returning one value */
    enum expect expect;
    sqlite3_int64 value;        /* EXPECT_INT: the expected result */
    int errcode;                /* EXPECT_ERROR: the expected extended error code */
    const char *errmsg;         /* EXPECT_ERROR: a substring of the error message */
};

#define T_INT(name, sql, n)           { name, sql, EXPECT_INT, n, 0, NULL }
#define T_NULL(name, sql)             { name, sql, EXPECT_NULL, 0, 0, NULL }
#define T_ERR(name, sql, code, msg)   { name, sql, EXPECT_ERROR, 0, code, msg }

static const struct test_case cases[] = {
    /* Distances */
    T_INT("identical integers",          "SELECT phhammdist(5, 5)", 0),
    T_INT("one bit apart (text)",        "SELECT phhammdist('1', '0')", 1),
    T_INT("all bits apart (text)",       "SELECT phhammdist('18446744073709551615', '0')", 64),
    T_INT("all bits apart (integer)",    "SELECT phhammdist(-1, 0)", 64),
    T_INT("alternating nibbles",         "SELECT phhammdist('17361641481138401520', '1085102592571150095')", 64),
    T_INT("top bit only (text)",         "SELECT phhammdist('9223372036854775808', '0')", 1),
    T_INT("top bit only (integer)",      "SELECT phhammdist(-9223372036854775808, 0)", 1),

    /* Signed INTEGER and unsigned TEXT forms of the same hash */
    T_INT("signed int equals unsigned text", "SELECT phhammdist('10720625987902197750', -7726118085807353866)", 0),

    /* Mixed argument types */
    T_INT("text vs integer",             "SELECT phhammdist('255', 0)", 8),
    T_INT("integer vs text",             "SELECT phhammdist(0, '255')", 8),
    T_INT("blob vs integer",             "SELECT phhammdist(x'00000000000000FF', 0)", 8),
    T_INT("blob vs text, same hash",     "SELECT phhammdist(x'94C7508922BB4FF6', '10720625987902197750')", 0),
    T_INT("blob is big-endian",          "SELECT phhammdist(x'0000000000000001', 1)", 0),
    T_INT("blob low byte is last",       "SELECT phhammdist(x'0100000000000000', 1)", 2),

    /* NULL in, NULL out */
    T_NULL("NULL first argument",        "SELECT phhammdist(NULL, 1)"),
    T_NULL("NULL second argument",       "SELECT phhammdist('x', NULL)"),
    T_NULL("both NULL",                  "SELECT phhammdist(NULL, NULL)"),

    /* TEXT that must be rejected */
    T_ERR("empty string",                "SELECT phhammdist('', '1')",     SQLITE_ERROR, "argument 1"),
    T_ERR("leading space",               "SELECT phhammdist('1', ' 1')",   SQLITE_ERROR, "argument 2"),
    T_ERR("plus sign",                   "SELECT phhammdist('+1', '1')",   SQLITE_ERROR, "argument 1"),
    T_ERR("minus sign",                  "SELECT phhammdist('-1', '1')",   SQLITE_ERROR, "argument 1"),
    T_ERR("not a number",                "SELECT phhammdist('abc', '1')",  SQLITE_ERROR, "argument 1"),
    T_ERR("trailing junk",               "SELECT phhammdist('123abc', 1)", SQLITE_ERROR, "argument 1"),
    T_ERR("hex text",                    "SELECT phhammdist('0x10', 1)",   SQLITE_ERROR, "argument 1"),
    T_ERR("2^64 out of range",           "SELECT phhammdist('18446744073709551616', 1)", SQLITE_ERROR, "argument 1"),
    T_ERR("60 digits out of range",      "SELECT phhammdist(1, '999999999999999999999999999999999999999999999999999999999999')", SQLITE_ERROR, "argument 2"),

    /* BLOB of the wrong length */
    T_ERR("7-byte blob",                 "SELECT phhammdist(x'01020304050607', 0)",     SQLITE_ERROR, "exactly 8 bytes"),
    T_ERR("9-byte blob",                 "SELECT phhammdist(0, x'010203040506070809')", SQLITE_ERROR, "argument 2"),
    T_ERR("empty blob",                  "SELECT phhammdist(x'', 0)",                   SQLITE_ERROR, "argument 1"),

    /* Unsupported type */
    T_ERR("REAL argument",               "SELECT phhammdist(1.5, 0)",  SQLITE_MISMATCH, "argument 1"),
    T_ERR("REAL second argument",        "SELECT phhammdist(0, 2.0)",  SQLITE_MISMATCH, "argument 2"),

    /* Argument count is fixed at 2 */
    T_ERR("one argument",                "SELECT phhammdist(1)",       SQLITE_ERROR, "wrong number of arguments"),
};

static int test_num = 0;
static int failures = 0;

/* Print one TAP result line, with an optional diagnostic on failure. */
static void tap(int ok, const char *name, const char *fmt, ...){
    test_num++;
    printf("%sok %d - %s", ok ? "" : "not ", test_num, name);
    if (!ok) {
        failures++;
        if (fmt != NULL) {
            va_list ap;
            printf("  # ");
            va_start(ap, fmt);
            vprintf(fmt, ap);
            va_end(ap);
        }
    }
    printf("\n");
}

/* Open an in-memory database and load the extension into it.
 * entry may be NULL to use the entry point SQLite derives from the file name. */
static sqlite3 *open_db(const char *ext, const char *entry, const char *encoding){
    sqlite3 *db = NULL;
    char *err = NULL;

    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        printf("Bail out! cannot open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }
    if (encoding != NULL) {
        char *sql = sqlite3_mprintf("PRAGMA encoding = '%q'", encoding);
        sqlite3_exec(db, sql, NULL, NULL, NULL);
        sqlite3_free(sql);
    }
    if (sqlite3_enable_load_extension(db, 1) != SQLITE_OK) {
        printf("Bail out! this SQLite library does not support loading extensions\n");
        exit(1);
    }
    if (sqlite3_load_extension(db, ext, entry, &err) != SQLITE_OK) {
        printf("Bail out! cannot load %s: %s\n", ext, err ? err : "unknown error");
        sqlite3_free(err);
        exit(1);
    }
    return db;
}

/* Run a one-value query, optionally binding text to ?1 with an explicit length,
 * and compare the result with the expectation. */
static void check_query(sqlite3 *db, const struct test_case *tc, const char *bind_text, int bind_len){
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, tc->sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        int ok = tc->expect == EXPECT_ERROR && strstr(sqlite3_errmsg(db), tc->errmsg) != NULL;
        tap(ok, tc->name, "prepare failed: %s", sqlite3_errmsg(db));
        return;
    }
    if (bind_text != NULL) {
        sqlite3_bind_text(stmt, 1, bind_text, bind_len, SQLITE_STATIC);
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int type = sqlite3_column_type(stmt, 0);
        sqlite3_int64 got = sqlite3_column_int64(stmt, 0);
        if (tc->expect == EXPECT_INT) {
            tap(type == SQLITE_INTEGER && got == tc->value, tc->name,
                "got type %d value %lld, expected %lld", type, (long long)got, (long long)tc->value);
        } else if (tc->expect == EXPECT_NULL) {
            tap(type == SQLITE_NULL, tc->name, "got type %d value %lld, expected NULL", type, (long long)got);
        } else {
            tap(0, tc->name, "got value %lld, expected error %d", (long long)got, tc->errcode);
        }
    } else {
        int code = sqlite3_extended_errcode(db);
        const char *msg = sqlite3_errmsg(db);
        int ok = tc->expect == EXPECT_ERROR && code == tc->errcode && strstr(msg, tc->errmsg) != NULL;
        tap(ok, tc->name, "got error %d '%s'", code, msg);
    }
    sqlite3_finalize(stmt);
}

/* Run SQL statements that should all succeed. */
static void check_exec(sqlite3 *db, const char *name, const char *sql){
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    tap(rc == SQLITE_OK, name, "error: %s", err ? err : "");
    sqlite3_free(err);
}

static void test_schema_objects(sqlite3 *db){
    /* SQLITE_INNOCUOUS: allowed in schema objects with trusted_schema=OFF.
     * SQLITE_DETERMINISTIC: required for generated columns and expression indexes. */
    check_exec(db, "trusted_schema=OFF: generated column and expression index",
        "PRAGMA trusted_schema = OFF;"
        "CREATE TABLE photos(id INTEGER PRIMARY KEY, hash,"
        "  dist0 INT GENERATED ALWAYS AS (phhammdist(hash, 0)));"
        "CREATE INDEX photos_dist ON photos(phhammdist(hash, 0));"
        "INSERT INTO photos(hash) VALUES ('255'), (NULL), (7), (x'00000000000000FF');");

    const struct test_case generated = T_INT("generated column value",
        "SELECT dist0 FROM photos WHERE id = 1", 8);
    check_query(db, &generated, NULL, 0);

    const struct test_case where_null = T_INT("NULL rows do not abort a WHERE scan",
        "SELECT count(*) FROM photos WHERE phhammdist(hash, '0') <= 8", 3);
    check_query(db, &where_null, NULL, 0);
}

static void test_unterminated_text(sqlite3 *db){
    /* Text bound with an explicit length and no NUL terminator. Only the
     * first byte belongs to the value. Reading past it (the old
     * sqlite3_value_blob() bug) parses extra digits and gives the wrong
     * distance; for the heap buffer it also reads out of bounds, which
     * `make test-valgrind` reports. (ASan does not see that read because it
     * happens inside uninstrumented libc.) */
    const struct test_case tc = T_INT("text without NUL terminator (stack)", "SELECT phhammdist(?1, '0')", 1);
    char stack_buf[] = "12345678";
    check_query(db, &tc, stack_buf, 1);

    const struct test_case tc_heap = T_INT("text without NUL terminator (heap)", "SELECT phhammdist(?1, '0')", 2);
    char *heap_buf = malloc(2);
    if (heap_buf == NULL) {
        printf("Bail out! out of memory\n");
        exit(1);
    }
    memcpy(heap_buf, "3", 1);
    heap_buf[1] = '9';
    check_query(db, &tc_heap, heap_buf, 1);
    free(heap_buf);
}

static void test_utf16(const char *ext){
    sqlite3 *db = open_db(ext, NULL, "UTF-16le");
    const struct test_case tc = T_INT("UTF-16 database",
        "SELECT phhammdist('18446744073709551615', '0')", 64);
    check_query(db, &tc, NULL, 0);
    sqlite3_close(db);
}

static void test_explicit_entry_point(const char *ext){
    sqlite3 *db = open_db(ext, "sqlite3_phhammdist_init", NULL);
    const struct test_case tc = T_INT("explicit entry point sqlite3_phhammdist_init",
        "SELECT phhammdist('1', '0')", 1);
    check_query(db, &tc, NULL, 0);
    sqlite3_close(db);
}

int main(int argc, char **argv){
    size_t i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <path-to-extension>\n", argv[0]);
        return 2;
    }

    /* Line-buffered so the last result before a crash is still printed. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    printf("# SQLite %s, extension %s\n", sqlite3_libversion(), argv[1]);

    /* Loaded with no entry point, which also tests the derived name. */
    sqlite3 *db = open_db(argv[1], NULL, NULL);
    tap(1, "load extension using the entry point derived from the file name", NULL);

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        check_query(db, &cases[i], NULL, 0);
    }
    test_schema_objects(db);
    test_unterminated_text(db);
    sqlite3_close(db);

    test_utf16(argv[1]);
    test_explicit_entry_point(argv[1]);

    printf("1..%d\n", test_num);
    if (failures > 0) {
        printf("# %d of %d tests failed\n", failures, test_num);
    }
    return failures > 0 ? 1 : 0;
}
