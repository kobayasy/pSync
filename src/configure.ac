# configure.ac - Last modified: 07-Mar-2026 (kobayasy)
#
# Copyright (C) 2018-2026 by Yuichi Kobayashi <kobayasy@kobayasy.com>
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

AC_INIT([pSync], [3.14], [kobayasy@kobayasy.com], [psync])
AC_COPYRIGHT([Copyright @{:@C@:}@ 2018-2026 by Yuichi Kobayashi <kobayasy@kobayasy.com>])
AC_PREREQ([2.72])
AC_CONFIG_SRCDIR([psync.c])
AC_CONFIG_AUX_DIR([conf])

AC_DEFUN([MY_CONFFILE_DEFAULT], [.psync.conf]) AC_SUBST([CONFFILE], [MY_CONFFILE_DEFAULT])
AC_DEFUN([MY_BACKUP_DEFAULT], [3])             AC_SUBST([BACKUP], [MY_BACKUP_DEFAULT])
AC_DEFUN([MY_EXPIRE_DEFAULT], [400])           AC_SUBST([EXPIRE], [MY_EXPIRE_DEFAULT])
AC_DEFUN([MY_SSH_DEFAULT], [ssh])              AC_SUBST([SSH], [MY_SSH_DEFAULT])
AC_DEFUN([MY_SSHOPTS_DEFAULT], [-qCp%u --])
AC_DEFUN([MY_SSHPORT_DEFAULT], [22])           AC_SUBST([SSHPORT], [MY_SSHPORT_DEFAULT])
AC_DEFUN([MY_TIMEOUT_DEFAULT], [disabled])

AC_DEFUN([MY_ARG_ENABLE],
   [AC_ARG_ENABLE([$1], [AS_HELP_STRING([--$2-$1], [$3])],
       [AS_CASE([$enableval],
         [yes|no],
           [],
           [AC_MSG_ERROR([invalid value for --$2-$1: $enableval @{:@must be yes or no@:}@])] )] )] )
AC_DEFUN([MY_ARG_WITH],
   [AC_ARG_WITH([$1], [AS_HELP_STRING([--with-$1=$2], [$3])],
       [AS_CASE([$withval],
         [yes|no],
           [AC_MSG_ERROR([--with-$1 requires an explicit argument])] )] )] )

AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_INSTALL

MY_ARG_ENABLE([warnall], [enable], [enable most reasonable warnings for compiler options])
AS_VAR_IF([enable_warnall], [yes],
   [AX_CFLAGS_WARN_ALL] )
AC_SYS_LARGEFILE
AX_PTHREAD(
   [AS_VAR_SET([CC], ["$PTHREAD_CC"])
    AX_APPEND_FLAG([$PTHREAD_CFLAGS])
    AS_VAR_SET([LIBS], ["$PTHREAD_LIBS $LIBS"]) ] )
AC_CHECK_FUNC([lutimes],
   [],
   [AC_DEFINE([MISSING_LUTIMES], [1], [Define to 1 if you are missing the 'lutimes' function.])] )
AC_CHECK_FUNC([clock_gettime],
   [],
   [AC_CHECK_LIB([rt], [clock_gettime])] )
MY_ARG_ENABLE([progress], [disable], [omit showing progress])
AS_VAR_IF([enable_progress], [no],
   [],
   [AC_CHECK_HEADERS([termcap.h],
       [AC_CHECK_FUNC([tgetent],
           [AC_DEFINE([HAVE_TGETENT], [1], [Define to 1 if you have the 'tgetent' function.])],
           [AC_SEARCH_LIBS([tgetent], [tinfo ncurses],
               [AC_DEFINE([HAVE_TGETENT], [1])] )] )] )] )
MY_ARG_WITH([conffile], [FILE], [configuration filename @<:@]MY_CONFFILE_DEFAULT[@:>@])
AS_VAR_SET_IF([with_conffile],
   [AC_DEFINE_UNQUOTED([CONFFILE], ["$with_conffile"], [Configuration filename.])
    AC_SUBST([CONFFILE], [$with_conffile]) ] )
AS_CASE([$CONFFILE],
 [/*],
   [],
   [AC_SUBST([CONFFILE], ["~/$CONFFILE"])] )
MY_ARG_WITH([backup], [DAYS], [retention period for backup files @<:@]MY_BACKUP_DEFAULT[@:>@])
AS_VAR_SET_IF([with_backup],
   [AC_DEFINE_UNQUOTED([BACKUP_DEFAULT], [@{:@$with_backup*24*60*60@:}@], [Retention period for backup files in seconds.])
    AC_SUBST([BACKUP], [$with_backup]) ] )
MY_ARG_WITH([expire], [DAYS], [retention period for file information @<:@]MY_EXPIRE_DEFAULT[@:>@])
AS_VAR_SET_IF([with_expire],
   [AC_DEFINE_UNQUOTED([EXPIRE_DEFAULT], [@{:@$with_expire*24*60*60@:}@], [Retention period for file information in seconds.])
    AC_SUBST([EXPIRE], [$with_expire]) ] )
MY_ARG_WITH([ssh], [FILE], [executable filename for SSH @<:@]MY_SSH_DEFAULT[@:>@])
AS_VAR_SET_IF([with_ssh],
   [AC_DEFINE_UNQUOTED([SSH], ["$with_ssh"], [Executable filename for SSH.])
    AC_SUBST([SSH], [$with_ssh]) ] )
AS_CASE([$SSH],
 [/*],
   [AC_SUBST([SSH_RPATH], ['./" '])],
   [AC_SUBST([SSH_APATH], ['./" '])] )
MY_ARG_WITH([sshopts], [OPTS], [command line options for SSH @<:@]MY_SSHOPTS_DEFAULT[@:>@])
AS_VAR_SET_IF([with_sshopts],
   [AC_DEFINE_UNQUOTED([SSHOPTS], ["$with_sshopts"], [Command line options to enable quiet and compress, and set port for SSH.])] )
MY_ARG_WITH([sshport], [PORT], [default port number for SSH @<:@]MY_SSHPORT_DEFAULT[@:>@])
AS_VAR_SET_IF([with_sshport],
   [AC_DEFINE_UNQUOTED([SSHPORT], [$with_sshport], [Default port number for SSH.])
    AC_SUBST([SSHPORT], [$with_sshport]) ] )
MY_ARG_WITH([timeout], [SECS], [communication timeout @<:@]MY_TIMEOUT_DEFAULT[@:>@])
AS_VAR_SET_IF([with_timeout],
   [AC_DEFINE_UNQUOTED([POLL_TIMEOUT], [$with_timeout], [Communication timeout in seconds.])] )
AC_CANONICAL_HOST
MY_ARG_ENABLE([universal2], [enable], [build universal binary @{:@arm64+x86_64@:}@ on macOS])
AS_VAR_IF([enable_universal2], [yes],
   [AS_CASE([$host_os],
     [darwin*],
       [AX_APPEND_FLAG([-arch arm64])
        AX_APPEND_FLAG([-arch x86_64]) ],
       [AC_MSG_ERROR([--enable-universal2 is supported only on macOS])] )] )

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AH_TOP([@%:@ifndef _INCLUDE_config_h
@%:@define _INCLUDE_config_h])
AH_BOTTOM([@%:@endif  /* @%:@ifndef _INCLUDE_config_h */])
AX_APPEND_FLAG([-I.], [CPPFLAGS])
AC_CONFIG_FILES([ja/psync.1 ja/psync.conf.5])
AC_OUTPUT
