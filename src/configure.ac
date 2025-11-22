# configure.ac - Last modified: 22-Nov-2025 (kobayasy)
#
# Copyright (C) 2018-2025 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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
AC_INIT([pSync], [3.2], [kobayasy@kobayasy.com], [psync])
AC_COPYRIGHT([Copyright @{:@C@:}@ 2018-2025 by Yuichi Kobayashi <kobayasy@kobayasy.com>])
AC_CONFIG_SRCDIR([psync.c])
AC_CONFIG_AUX_DIR([conf])

AC_SUBST([CONFFILE], ['~/.psync.conf'])
AC_SUBST([BACKUP], ['3'])
AC_SUBST([EXPIRE], ['400'])
AC_SUBST([SSH], ['ssh'])
AC_SUBST([SSHPORT], ['22'])

AC_DEFUN([MY_ARG_ENABLE],
   [AC_ARG_ENABLE([$1], [AS_HELP_STRING([--$2-$1], [$3])],
       [AS_CASE([$enableval],
         [yes|no],
           [],
           [AC_MSG_WARN([invalid option: --enable-$1=$enableval])
            AS_UNSET([enableval]) ] )],
       [AS_UNSET([enableval])] )] )
AC_DEFUN([MY_ARG_WITH],
   [AC_ARG_WITH([$1], [AS_HELP_STRING([--with-$1=$2], [$3])],
       [AS_CASE([$withval],
         [yes|no],
           [AC_MSG_WARN([invalid option: --with-$1=$withval])
            AS_UNSET([withval]) ] )],
       [AS_UNSET([withval])] )] )

AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_INSTALL

MY_ARG_ENABLE([warnall], [enable], [enable most reasonable warnings for compiler options])
AS_VAR_IF([enableval], [yes],
   [AX_CFLAGS_WARN_ALL] )
AC_SYS_LARGEFILE
AX_PTHREAD([LIBS="$PTHREAD_LIBS $LIBS" CFLAGS="$CFLAGS $PTHREAD_CFLAGS" CC="$PTHREAD_CC"])
AC_CHECK_FUNC([clock_gettime],
   [],
   [AC_CHECK_LIB([rt], [clock_gettime])] )
MY_ARG_ENABLE([progress], [disable], [omit showing progress])
AS_VAR_IF([enableval], [no],
   [],
   [AC_CHECK_HEADERS([termcap.h],
       [AC_CHECK_FUNC([tgetent],
           [AC_DEFINE([HAVE_TGETENT], [1], [Define to 1 if you have the `tgetent' function.])],
           [AC_SEARCH_LIBS([tgetent], [tinfo ncurses],
               [AC_DEFINE([HAVE_TGETENT], [1])] )] )] )] )
MY_ARG_WITH([conffile], [FILE], [configuration filename @{:@default: .psync.conf@:}@])
AS_VAR_SET_IF([withval],
   [AC_DEFINE_UNQUOTED([CONFFILE], ["$withval"], [Configuration filename.])
    AS_CASE([$withval],
     [/*],
       [AC_SUBST([CONFFILE], [$withval])],
       [AC_SUBST([CONFFILE], ['~/'$withval])] ) ] )
MY_ARG_WITH([backup], [DAYS], [retention period for backup files @{:@default: 3@:}@])
AS_VAR_SET_IF([withval],
   [AC_DEFINE_UNQUOTED([BACKUP_DEFAULT], [@{:@$withval*24*60*60@:}@], [Retention period for backup files in seconds.])
    AC_SUBST([BACKUP], [$withval]) ] )
MY_ARG_WITH([expire], [DAYS], [retention period for file information @{:@default: 400@:}@])
AS_VAR_SET_IF([withval],
   [AC_DEFINE_UNQUOTED([EXPIRE_DEFAULT], [@{:@$withval*24*60*60@:}@], [Retention period for file information in seconds.])
    AC_SUBST([EXPIRE], [$withval]) ] )
MY_ARG_WITH([ssh], [FILE], [executable filename for SSH @{:@default: ssh@:}@])
AS_VAR_SET_IF([withval],
   [AC_DEFINE_UNQUOTED([SSH], ["$withval"], [Executable filename for SSH.])
    AC_SUBST([SSH], [$withval])
    AS_CASE([$withval],
     [/*],
       [AC_SUBST([SSH_PATH], ['./" '])] ) ] )
MY_ARG_WITH([sshopts], [OPTS], [command line options for SSH @{:@default: -qCp%u@:}@])
AS_VAR_SET_IF([withval],
   [AC_DEFINE_UNQUOTED([SSHOPTS], ["$withval"], [Command line options to enable quiet and compress, and set port for SSH.])] )
MY_ARG_WITH([sshport], [PORT], [default port number for SSH @{:@default: 22@:}@])
AS_VAR_SET_IF([withval],
   [AC_DEFINE_UNQUOTED([SSHPORT], [$withval], [Default port number for SSH.])
    AC_SUBST([SSHPORT], [$withval]) ] )

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AH_TOP([#ifndef _INCLUDE_config_h
#define _INCLUDE_config_h])
AH_BOTTOM([#endif  /* #ifndef _INCLUDE_config_h */])
AX_APPEND_FLAG([-I.])
AC_CONFIG_FILES([ja/psync.1 ja/psync.conf.5])
AC_OUTPUT
