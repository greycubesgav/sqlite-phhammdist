# sqlite-phhammdist
PHash Hamming distance calcuation in SQLite

sqlite-phhammdist - Hamming distance between two unsigned 64-bit hashes in SQLite returned as an INT

## Synopsis
This SQLite extension adds a function phhammdist() to SQLite.  phhammdist()
takes two unsigned 64-bit hashes (as INTEGER, TEXT or BLOB) and returns the
Hamming distance as an integer, as per the pHash cpp implementation.

## Usage
To load the extension into an open SQLite database, run the following queries:

```SQL
  SELECT load_extension('/path/to/sqlite-phhammdist.so');
```

or, in the `sqlite3` shell, `.load /path/to/sqlite-phhammdist`. The entry point
SQLite derives from the file name is exported, so it does not need to be named.
The explicit form `load_extension('/path/to/sqlite-phhammdist.so',
'sqlite3_phhammdist_init')` also still works, and is needed if you rename the
library file.


You can then use the function in SQL queries, for example to find photos which
have a perceptual hash that is close in Hamming distance:

```SQL
  SELECT photo_id, phhammdist(photo_hash, ?) AS ph_dist FROM photos WHERE ph_dist <= 9;
```

Each argument is converted independently, so they need not be the same type:

* **INTEGER**: SQLite's signed 64-bit value, reinterpreted as unsigned
  (`-7726118085807353866` is the same hash as `10720625987902197750`).
* **TEXT**: a plain unsigned decimal integer that fits in 64 bits
  (e.g. `'10720625987902197750'`). Empty strings, signs, whitespace, trailing
  characters and out-of-range values raise an error rather than being
  silently converted.
* **BLOB**: exactly 8 bytes, big-endian (most significant byte first), the
  most compact way to store a 64-bit hash.
* **NULL**: if either argument is NULL the result is NULL, so rows with a
  missing hash simply don't match instead of aborting the query.

Any other type (e.g. REAL) raises an `SQLITE_MISMATCH` error.

The function is registered as `SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS`, so it
can be used in views, triggers, CHECK constraints, generated columns and
indexes even when `PRAGMA trusted_schema=OFF`.

## Building
The source requires a C99 compiler. Run `make` (GNU make). This builds
`sqlite-phhammdist.so` (`.dylib` on macOS, `.dll` on Windows with MinGW) and a
debug build, `sqlite-phhammdist_debug.so` (compiled with `-DPHHAMMDIST_DEBUG`).
On macOS the library is built as a universal (arm64 + x86_64) binary. Only the
SQLite headers are needed: a loadable extension must not link against
libsqlite3.

**Never deploy the debug build.** It writes argument values to the host
process's stderr on every call, which leaks query data into application logs
and adds I/O to every row. Use it for local debugging only.

With MSVC, from a Developer Command Prompt with the SQLite headers on the
include path:

```
  cl /O2 /LD sqlite-phhammdist.c /Fesqlite-phhammdist.dll
```

To compile the extension directly into an application instead of loading it
at runtime, build `sqlite-phhammdist.c` with `-DSQLITE_CORE` and register it
for every new connection before opening any:

```C
  sqlite3_auto_extension((void (*)(void))sqlite3_phhammdist_init);
```

## Loading from Python
When loading extensions from python, the python binary needs to be built with
the --enable-loadable-sqlite-extensions configure argument.  You then need to
enable the loading of extensions using:

```
  dbconn.enable_load_extension(True)
```

After that, you can issue the above SELECT statement to load the extension.

## Inspiration
Inspiration for this comes from:

https://github.com/droe/sqlite-hexhammdist  
Hamming Distance calcation in SQL Lite using hex representations from Daniel Roethlisberger

https://gist.github.com/kylelk/39fed416b0125dbbe62e  
Population bit count calcuation in SQL Lite from kyle kersey

https://gitlab.com/opennota/findimagedupes  
Find image duplicates written in golang from opennota
