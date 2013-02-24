# wikattrib

This program processes a MediaWiki full dump file to create a set of pages that
can be used to easily identify the original author of any word on the page.

While this program works, it is at the "proof of concept" stage and should not 
be considered production-ready.

## Input file

The input to this program is the 7zip format MediaWiki XML dump file with all
historical revisions. On the Wikimedia dump page http://dumps.wikimedia.org/backup-index.html
this is the file whose name ends with `-pages-meta-history.xml.7z`.

## Instructions

There are two parts to this program. The first is C++ and is called `dumpsplit`
(it can be built using [SCons](http://scons.org) with the included `SConstruct` file,
or directly with something like `gcc -o dumpsplit dumpsplit.cpp`). This splits the full
dump file into a set of files, one file per page.

    mkdir out
    ./dumpsplit simplewiki-20130203-pages-meta-history.xml.7z out

The above will create a few thousand `.xz` files in the `out/` subdirectory.
Then, the `wikattrib.py` script will create the HTML attribution output for a single page:

    mkdir html
    python wikattrib.py simple out Aardvark html

The `go` script can be used to run this for every file in the `out` directory.

## License

This program is licensed under the GPL version 2 and is intended to be the same license
as MediaWiki itself.
