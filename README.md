# sqlite-phhammdist
[![CI](https://github.com/greycubesgav/sqlite-phhammdist/actions/workflows/ci.yml/badge.svg)](https://github.com/greycubesgav/sqlite-phhammdist/actions/workflows/ci.yml)

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

## Downloads
Prebuilt libraries are attached to each
[release](https://github.com/greycubesgav/sqlite-phhammdist/releases):

| File | Platform |
|---|---|
| `sqlite-phhammdist-<version>-linux-x86_64.tar.gz` | Linux x86_64 (glibc 2.4 or later) |
| `sqlite-phhammdist-<version>-linux-arm64.tar.gz` | Linux arm64 |
| `sqlite-phhammdist-<version>-macos-universal.tar.gz` | macOS 11 or later, Apple Silicon and Intel |
| `sqlite-phhammdist-<version>-windows-x64.zip` | Windows x64 (no Visual C++ Redistributable needed) |

Keep the library's file name as it is: SQLite derives the entry point from it.
To check a download, compare it with the release's `SHA256SUMS` file, or
verify that it was built by this repository's release workflow with the
[GitHub CLI](https://cli.github.com/):

```
  sha256sum --ignore-missing -c SHA256SUMS
  gh attestation verify sqlite-phhammdist-<version>-linux-x86_64.tar.gz -R greycubesgav/sqlite-phhammdist
```

## Installing on Linux
`make install` (as root) copies `sqlite-phhammdist.so` into a directory the
dynamic loader searches. On Debian, Ubuntu and other multiarch distributions
that is `/usr/lib/<arch>-linux-gnu/` (e.g. `/usr/lib/x86_64-linux-gnu/`).
Other distributions use `/usr/lib64` or `/usr/lib`. You can then load the
extension by name, without a path:

```
  sudo make install
```

```SQL
  SELECT load_extension('sqlite-phhammdist');
```

or `.load sqlite-phhammdist` in the `sqlite3` shell. SQLite never loads
extensions automatically; to load it in every `sqlite3` shell session, add
that `.load` line to `~/.sqliterc`.

`sudo make uninstall` removes it. Set `LIBDIR=...` to install somewhere else,
and `DESTDIR=...` to stage the install into a package build root. For a
release download, copy the `.so` from the archive into the same directory.

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

## Testing
`make test` builds the extension and a small C test driver
(`tests/test_phhammdist.c`, linked against libsqlite3), then runs the suite
twice: once against the normal build and once against a build that uses the
portable popcount fallback. Results are printed in
[TAP](https://testanything.org/) format, so `prove` and most CI systems can
read them, and `make test` fails if any test fails.

```
  make test            # normal run
  make test-asan       # with AddressSanitizer and UndefinedBehaviorSanitizer
  make test-valgrind   # under valgrind, with leak checking
  prove --exec ./tests/test_phhammdist ./sqlite-phhammdist.so   # via prove, after make test
```

GitHub Actions runs the suite on every pull request and push to `master`:
on Linux with gcc and clang (including ASan/UBSan and valgrind), on macOS
(universal build, ASan/UBSan), and on Windows with both MinGW and MSVC
(see `.github/workflows/ci.yml`).

The test driver needs a SQLite library that can load extensions. macOS's
system libsqlite3 cannot, so on macOS install SQLite with Homebrew and run
`make test SQLITE_PREFIX=$(brew --prefix sqlite)`.

To add a test, add one line to the `cases[]` table in
`tests/test_phhammdist.c`, using `T_INT(name, sql, expected)`,
`T_NULL(name, sql)` or `T_ERR(name, sql, sqlite_error_code, message_substring)`.

## Making a release
Push a version tag, e.g. `git tag v1.2.0 && git push origin v1.2.0`. The
[Release workflow](.github/workflows/release.yml) builds and tests the
extension on every platform above, then creates a **draft** release with the
archives, a `SHA256SUMS` file, build attestations and generated release notes.
Review the draft on the Releases page, edit the notes if needed, and publish
it. Tags with a suffix (`v1.2.0-rc.1`) become pre-releases.

To release a tag that already exists, or re-run a failed release, use
**Actions → Release → Run workflow** and enter the tag. Re-running only
updates a release that is still a draft; a published release is never
changed.

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
