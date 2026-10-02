# sqlite-phhammdist
PHash Hamming distance calcuation in SQLite

sqlite-phhammdist - Hamming distance between two UINT64 TEXT string in SQLite returned as an INT

## Synopsis
This SQLite extension adds a function phhammdist() to SQLite.  phhammdist()
takes two unsigned 64bit integer in TEXT string as arguments and returns the 
Hamming distance as an integer, as per the pHash cpp implementation.

## Usage
To load the extension into an open SQLite database, run the following queries:

```SQL
  SELECT load_extension('/path/to/sqlite-phhammdist.so', 'sqlite3_phhammdist_init');
```


You can then use the function in SQL queries, for example to find photos which
have a perceptual hash that is close in Hamming distance:

```SQL
  SELECT photo_id, phhammdist(photo_hash, ?) AS ph_dist FROM photos WHERE ph_dist <= 9;
```

Both arguments must be TEXT or both must be INTEGER. TEXT arguments must be
plain unsigned decimal integers that fit in 64 bits (e.g. `'10720625987902197750'`):
empty strings, signs, whitespace, trailing characters and out-of-range values
raise an error rather than being silently converted. INTEGER arguments are
SQLite's signed 64-bit values and are reinterpreted as unsigned.

The function is registered as `SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS`, so it
can be used in views, triggers, CHECK constraints, generated columns and
indexes even when `PRAGMA trusted_schema=OFF`.

## Building
Run `make`. This builds `sqlite-phhammdist.so` and a debug build,
`sqlite-phhammdist_debug.so` (compiled with `-DPHHAMMDIST_DEBUG`).

**Never deploy the debug build.** It writes argument values to the host
process's stderr on every call, which leaks query data into application logs
and adds I/O to every row. Use it for local debugging only.

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
