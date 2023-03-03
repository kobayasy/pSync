# configure.ac - Last modified: 03-Mar-2023 (kobayasy)
#
# Copyright (c) 2018-2023 by Yuichi Kobayashi <kobayasy@kobayasy.com>
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

AC_PREREQ(2.69)
AC_INIT([pSync], [2.28], [kobayasy@kobayasy.com], [psync], [http://kobayasy.com/psync/])
AC_CONFIG_SRCDIR([psync.c])
AC_CONFIG_AUX_DIR([conf])

AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_INSTALL

AC_SYS_LARGEFILE
AX_PTHREAD([LIBS="$PTHREAD_LIBS $LIBS" CFLAGS="$CFLAGS $PTHREAD_CFLAGS" CC="$PTHREAD_CC"])
AC_CHECK_FUNC([clock_gettime],
   [],
   [AC_CHECK_LIB([rt], [clock_gettime])] )
AC_ARG_ENABLE([progress], [AS_HELP_STRING([--disable-progress], [omit showing progress])],
   [],
   [enableval=yes] )
AS_VAR_IF([enableval], [no],
   [],
   [AC_CHECK_HEADERS([termcap.h],
       [AC_CHECK_FUNC([tgetent],
           [AC_DEFINE([HAVE_TGETENT], [1], [Define to 1 if you have the `tgetent' function.])],
           [AC_SEARCH_LIBS([tgetent], [tinfo ncurses],
               [AC_DEFINE([HAVE_TGETENT], [1])] )] )] )] )
AC_ARG_ENABLE([sshpath], [AS_HELP_STRING([--enable-sshpath@<:@=PATH@:>@], [run ssh with absolute path @{:@default: use @S|@PATH@:}@])],
   [],
   [enableval=no] )
AS_CASE([$enableval],
   [yes],
       [AC_PATH_PROG([SSH], [ssh])
        AS_VAR_IF([SSH], [],
           [],
           [AC_DEFINE_UNQUOTED([SSH], ["$SSH"], [Absolute path name of ssh.])] ) ],
   [no],
       [],
   [AC_DEFINE_UNQUOTED([SSH], ["$enableval"])] )
AC_ARG_ENABLE([sshopts], [AS_HELP_STRING([--enable-sshopts=OPTS], [run options for ssh @{:@default: -qCp%u@:}@])],
   [],
   [enableval=no] )
AS_CASE([$enableval],
   [yes],
       [],
   [no],
       [],
   [AC_DEFINE_UNQUOTED([SSHOPTS], ["$enableval"], [ssh options to enable quiet and compress, and set port.])] )

AC_CONFIG_HEADERS([config.h])
AH_TOP([#ifndef _INCLUDE_config_h
#define _INCLUDE_config_h])
AH_BOTTOM([#endif  /* #ifndef _INCLUDE_config_h */])
AS_VAR_APPEND([CFLAGS], [' -I.'])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
