#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([hayk], [0.1], [xsisqox@gmail.co])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_create)

PKG_CHECK_MODULES([LIBCONFIGXX], [libconfig++ >= 1.4],,
       AC_MSG_ERROR([libconfig++ 1.4 or newer not found.])
     )

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h stdlib.h sys/socket.h openssl/sha.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([inet_ntoa socket])

AC_CONFIG_FILES([Makefile
				 src/Makefile
				 src/lib/Makefile
				 conf/Makefile])
AC_OUTPUT
