AC_INIT([eselect-python], [20100321])

CFLAGS="${CFLAGS--march=native -O3 -pipe -fno-ident}"
LDFLAGS="${LDFLAGS--Wl,-O1,--as-needed,--gc-sections,--hash-style=gnu}"

AC_PROG_CC
AC_PATH_PROG(INSTALL, install)
MKDIR="${MKDIR:-${INSTALL} -d}"

AC_USE_SYSTEM_EXTENSIONS

# setenv() was introduced in POSIX.1-2008.
# strtok_r() was introduced in POSIX.1-2001.
AC_CHECK_FUNCS([setenv strtok_r])

# strndup() was introduced in POSIX.1-2008 and is also an implicitly declared built-in function in GCC.
AC_MSG_CHECKING([for strndup])
old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wall -Werror -Wextra"
AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <string.h>
int main()
{
  strndup("", 0);
  return 0;
}]])], [have_strndup="1"], [have_strndup="0"])
if test "${have_strndup}" = "1"; then
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_STRNDUP], [1], [Define to 1 if you have the 'strndup' function.])
else
  AC_MSG_RESULT([no])
fi
CFLAGS="${old_CFLAGS}"

AC_SUBST([MKDIR])

# Create output files.
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
