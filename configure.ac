dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([matrix], [0.0.1])

AC_CONFIG_SRCDIR([src/matrix.c])
# AC_CONFIG_MACRODIR([m4])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE([subdir-objects foreign dist-bzip2])

AC_SUBST(VERSION)

ISODATE=`date +%Y-%m-%d`
AC_SUBST(ISODATE)

AC_SUBST(INTI_CFLAGS)
AC_SUBST(INTI_LIBS)

AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC
AM_PROG_AS

dnl Checks for libraries.

dnl Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_MAJOR
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h libintl.h locale.h memory.h stdint.h stdlib.h string.h strings.h sys/file.h sys/param.h sys/time.h unistd.h utime.h])

#jm_CHECK_TYPE_STRUCT_UTIMBUF
AC_HEADER_MAJOR
AC_FUNC_ALLOCA
AC_STRUCT_TM
AC_STRUCT_ST_BLOCKS
AC_FUNC_CLOSEDIR_VOID
AC_CHECK_FUNCS(mkfifo)
AC_CHECK_FUNC(mknod)
AC_CHECK_HEADER(argp.h,,AC_MSG_ERROR(Cannot continue: argp.h not found))
    
dnl Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

dnl Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memchr memset])

AC_OUTPUT([Makefile src/Makefile src/matrix.lsm src/matrix.spec])



