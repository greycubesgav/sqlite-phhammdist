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
