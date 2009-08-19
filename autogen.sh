#!/bin/sh

"${AUTOHEADER:-autoheader}" || exit 1
"${AUTOCONF:-autoconf}" || exit 1

# Remove Autoconf cache.
rm -fr autom4te.cache
