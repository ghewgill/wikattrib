#!/bin/sh

for a in `cd out; ls`; do
    b=`echo $a|sed 's/.xz//'`
    if [ -n "$ALL" -o ! -f "html/$b.html" ]; then
        python wikiblame.py simple out "$b" html
    fi
done
