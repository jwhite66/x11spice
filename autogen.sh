#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

set -e # exit on errors

autoreconf --verbose --install

if [ -z "$NOCONFIGURE" ]; then
    "$srcdir"/configure "$@"
fi
