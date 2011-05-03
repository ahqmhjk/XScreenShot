#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.65)
AC_INIT([XScreenShot],[1.0],[bug-report@gnu.org])
AC_CONFIG_SRCDIR([src/g_save.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE(XScreenShot, 1.0, bug-report@gnu.org)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
LT_INIT

# Checks for libraries.

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h strings.h sys/param.h unistd.h utime.h])
AC_CHECK_HEADERS([X11/Xlib.h X11/Xutil.h X11/Xatom.h X11/cursorfont.h jpeglib.h png.h tiffio.h setjmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset strchr utime])

AC_CONFIG_FILES([Makefile inc/Makefile src/Makefile])
AC_OUTPUT
