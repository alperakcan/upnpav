#
# M4 Commands
#

dnl @synopsis AC_C_BIGENDIAN_CROSS
dnl
dnl Check endianess even when crosscompiling
dnl (partially based on the original AC_C_BIGENDIAN).
dnl
dnl The implementation will create a binary, and instead of running
dnl the binary it will be grep'ed for some symbols that will look
dnl different for different endianess of the binary.
dnl
dnl @version $Id: ac_c_bigendian_cross.m4,v 1.2 2001/10/01 20:03:13 fdrake Exp $
dnl @author Guido Draheim <guidod@gmx.de>
dnl
AC_DEFUN([AC_C_BIGENDIAN_CROSS],
[AC_CACHE_CHECK(whether byte ordering is bigendian, ac_cv_c_bigendian,
[ac_cv_c_bigendian=unknown
# See if sys/param.h defines the BYTE_ORDER macro.
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/param.h>], [
#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
 bogus endian macros
#endif], [# It does; now see whether it defined to BIG_ENDIAN or not.
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/param.h>], [
#if BYTE_ORDER != BIG_ENDIAN
 not big endian
#endif], ac_cv_c_bigendian=yes, ac_cv_c_bigendian=no)])
if test $ac_cv_c_bigendian = unknown; then
AC_TRY_RUN([main () {
  /* Are we little or big endian?  From Harbison&Steele.  */
  union
  {
    long l;
    char c[sizeof (long)];
  } u;
  u.l = 1;
  exit (u.c[sizeof (long) - 1] == 1);
}], ac_cv_c_bigendian=no, ac_cv_c_bigendian=yes,
[ echo $ac_n "cross-compiling... " 2>&AC_FD_MSG ])
fi])
if test $ac_cv_c_bigendian = unknown; then
AC_MSG_CHECKING(to probe for byte ordering)
[
cat >conftest.c <<EOF
short ascii_mm[] = { 0x4249, 0x4765, 0x6E44, 0x6961, 0x6E53, 0x7953, 0 };
short ascii_ii[] = { 0x694C, 0x5454, 0x656C, 0x6E45, 0x6944, 0x6E61, 0 };
void _ascii() { char* s = (char*) ascii_mm; s = (char*) ascii_ii; }
short ebcdic_ii[] = { 0x89D3, 0xE3E3, 0x8593, 0x95C5, 0x89C4, 0x9581, 0 };
short ebcdic_mm[] = { 0xC2C9, 0xC785, 0x95C4, 0x8981, 0x95E2, 0xA8E2, 0 };
void _ebcdic() { char* s = (char*) ebcdic_mm; s = (char*) ebcdic_ii; }
int main() { _ascii (); _ebcdic (); return 0; }
EOF
] if test -f conftest.c ; then
     if ${CC-cc} -c conftest.c -o conftest.o && test -f conftest.o ; then
        if test `grep -l BIGenDianSyS conftest.o` ; then
           echo $ac_n ' big endian probe OK, ' 1>&AC_FD_MSG
           ac_cv_c_bigendian=yes
        fi
        if test `grep -l LiTTleEnDian conftest.o` ; then
           echo $ac_n ' little endian probe OK, ' 1>&AC_FD_MSG
           if test $ac_cv_c_bigendian = yes ; then
            ac_cv_c_bigendian=unknown;
           else
            ac_cv_c_bigendian=no
           fi
        fi
        echo $ac_n 'guessing bigendian ...  ' >&AC_FD_MSG
     fi
  fi
AC_MSG_RESULT($ac_cv_c_bigendian)
fi
if test $ac_cv_c_bigendian = yes; then
  AC_DEFINE(WORDS_BIGENDIAN, 1, [whether byteorder is bigendian])
  BYTEORDER=4321
else
  BYTEORDER=1234
fi
AC_DEFINE_UNQUOTED(BYTEORDER, $BYTEORDER, [1234 = LIL_ENDIAN, 4321 = BIGENDIAN])
if test $ac_cv_c_bigendian = unknown; then
  AC_MSG_ERROR(unknown endianess - sorry, please pre-set ac_cv_c_bigendian)
fi
])

dnl @synopsis VL_LIB_READLINE
dnl
dnl Searches for a readline compatible library. If found, defines
dnl `HAVE_LIBREADLINE'. If the found library has the `add_history'
dnl function, sets also `HAVE_READLINE_HISTORY'. Also checks for the
dnl locations of the necessary include files and sets `HAVE_READLINE_H'
dnl or `HAVE_READLINE_READLINE_H' and `HAVE_READLINE_HISTORY_H' or
dnl 'HAVE_HISTORY_H' if the corresponding include files exists.
dnl
dnl The libraries that may be readline compatible are `libedit',
dnl `libeditline' and `libreadline'. Sometimes we need to link a
dnl termcap library for readline to work, this macro tests these cases
dnl too by trying to link with `libtermcap', `libcurses' or
dnl `libncurses' before giving up.
dnl This macro sets the READLINE_LIBS variable accordingly, and AC_SUBST it.
dnl
dnl Here is an example of how to use the information provided by this
dnl macro to perform the necessary includes or declarations in a C
dnl file:
dnl
dnl   #ifdef HAVE_LIBREADLINE
dnl   #  if defined(HAVE_READLINE_READLINE_H)
dnl   #    include <readline/readline.h>
dnl   #  elif defined(HAVE_READLINE_H)
dnl   #    include <readline.h>
dnl   #  else /* !defined(HAVE_READLINE_H) */
dnl   extern char *readline ();
dnl   #  endif /* !defined(HAVE_READLINE_H) */
dnl   char *cmdline = NULL;
dnl   #else /* !defined(HAVE_READLINE_READLINE_H) */
dnl     /* no readline */
dnl   #endif /* HAVE_LIBREADLINE */
dnl
dnl   #ifdef HAVE_READLINE_HISTORY
dnl   #  if defined(HAVE_READLINE_HISTORY_H)
dnl   #    include <readline/history.h>
dnl   #  elif defined(HAVE_HISTORY_H)
dnl   #    include <history.h>
dnl   #  else /* !defined(HAVE_HISTORY_H) */
dnl   extern void add_history ();
dnl   extern int write_history ();
dnl   extern int read_history ();
dnl   #  endif /* defined(HAVE_READLINE_HISTORY_H) */
dnl     /* no history */
dnl   #endif /* HAVE_READLINE_HISTORY */
dnl
dnl @category InstalledPackages
dnl @author Ville Laurikari <vl@iki.fi>
dnl @version 2002-04-04-patched:
dnl	patch 2005-11-06 by R. Turboult : 
dnl		1) output READLINE_LIBS instead of LIBS
dnl		2) correct behaviour when cache is used (LIBS was not set ...)
dnl @license AllPermissive

AC_DEFUN([VL_LIB_READLINE], [
  AC_CACHE_CHECK([for a readline compatible library],
                 vl_cv_lib_readline, [
    ORIG_LIBS="$LIBS"
    for readline_lib in readline edit editline; do
      for termcap_lib in "" termcap curses ncurses; do
        if test -z "$termcap_lib"; then
          TRY_LIB="-l$readline_lib"
        else
          TRY_LIB="-l$readline_lib -l$termcap_lib"
        fi
        LIBS="$ORIG_LIBS $TRY_LIB"
        AC_TRY_LINK_FUNC(readline, vl_cv_lib_readline="$TRY_LIB")
        if test -n "$vl_cv_lib_readline"; then
          break
        fi
      done
      if test -n "$vl_cv_lib_readline"; then
        break
      fi
    done
    if test -z "$vl_cv_lib_readline"; then
      vl_cv_lib_readline="no"
    fi
    # patch R. Turboult 2005-11-06
    LIBS="$ORIG_LIBS"
  ])


  if test "$vl_cv_lib_readline" != "no"; then

    # patch R. Turboult 2005-11-06
    READLINE_LIBS="$vl_cv_lib_readline"

    AC_DEFINE(HAVE_LIBREADLINE, 1,
              [Define if you have a readline compatible library])
    AC_CHECK_HEADERS(readline.h readline/readline.h)
    AC_CACHE_CHECK([whether readline supports history],
                   vl_cv_lib_readline_history, [
      vl_cv_lib_readline_history="no"
      # patch R. Turboult 2005-11-06
      ORIG_LIBS="$LIBS"
      LIBS="$ORIG_LIBS $READLINE_LIBS"
      AC_TRY_LINK_FUNC(add_history, vl_cv_lib_readline_history="yes")
      LIBS="$ORIG_LIBS"
    ])
    if test "$vl_cv_lib_readline_history" = "yes"; then
      AC_DEFINE(HAVE_READLINE_HISTORY, 1,
                [Define if your readline library has \`add_history'])
      AC_CHECK_HEADERS(history.h readline/history.h)
    fi
  fi

  # patch R. Turboult 2005-11-06
  AC_SUBST(READLINE_LIBS)

])

AC_INIT([upnpd], [0.0.2], [upnpd@alperakcan.org])
AC_CANONICAL_TARGET

#AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADER([config.h])

# Initialize config.h
AC_GNU_SOURCE

# Enable maintainer mode
AM_MAINTAINER_MODE

# Disable shared
AC_DISABLE_SHARED

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_RANLIB

AC_PATH_PROG(CHMOD, chmod, :)

case $target_os in
     *linux*)	arch=linux;;
     *darwin*)	arch=darwin;;
     *)		arch=unknown;;
esac

# Endian test
AC_C_BIGENDIAN_CROSS

# Large file support
#AC_SYS_LARGEFILE
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_DEFINE([_LARGE_FILE_SOURCE], [], [Large files support])
AC_DEFINE([_FILE_OFFSET_BITS], [64], [File Offset size])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_CHECK_HEADERS([ \
	stdio.h \
	stdlib.h \
	string.h \
	fcntl.h \
	unistd.h \
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([ \
	printf \
	strcmp \
	memset \
	memmove \
	bcopy \
])

CFLAGS="$CFLAGS -Wall -Werror"

#
# dependencies
#

# - pkgconfig

AC_ARG_WITH(pkgconfigdir,
            [  --with-pkgconfigdir=DIR pkgconfig file in DIR @<:@LIBDIR/pkgconfig@:>@],
            [pkgconfigdir=$withval],
            [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST(pkgconfigdir)


# - readline

VL_LIB_READLINE

# - fuse

PKG_CHECK_MODULES(LIBFUSE, fuse, HAVE_LIBFUSE=yes, HAVE_LIBFUSE=no)
if test "$HAVE_LIBFUSE" = "yes"
then
	enable_libfuse=yes
else
	enable_libfuse=no
fi

AC_MSG_CHECKING([if FUSE on this system is too new for us])
AC_EGREP_CPP([fuse_version_yes], [
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 27
#include "fuse.h"
#if FUSE_VERSION > 27
fuse_version_yes
#endif
], AC_DEFINE([FUSE_USE_VERSION], [27], [Version of FUSE interface]) AC_MSG_RESULT([yes]),
   AC_DEFINE([FUSE_USE_VERSION], [FUSE_VERSION], [Version of FUSE interface]) AC_MSG_RESULT([no]))

# - ffmpeg

PKG_CHECK_MODULES(LIBFFMPEG, libavformat libavcodec libswscale libavutil, HAVE_LIBFFMPEG=yes, HAVE_LIBFFMPEG=no)
if test "$HAVE_LIBFFMPEG" = "yes"
then
	enable_libffmpeg=yes
	LIBFFMPEG_LIBS="$LIBFFMPEG_LIBS -lz -lbz2 -lm"
	AC_DEFINE(HAVE_LIBFFMPEG, 1, [Define to 1 if you want 'libffmpeg' support.])
else
	enable_libffmpeg=no
fi

#
# Check extra parameters
#

AC_ARG_ENABLE([platform],
[  --enable-platform=[[platform]] platform can be one of [[posix (default),corec]]],
if test "$enableval" = "posix"
then
	enable_platform_posix=yes
	enable_platform_corec=no
	AC_DEFINE(ENABLE_PLATFORM_POSIX, 1, [Define to 1 if you want 'posix' platform support.])
elif test "$enableval" = "corec"
then
	enable_platform_corec=yes
	enable_platform_posix=no
	AC_DEFINE(ENABLE_PLATFORM_COREC, 1, [Define to 1 if you want 'corec' platform support.])
else
	enable_platform_posix=yes
	enable_platform_corec=no
	AC_DEFINE(ENABLE_PLATFORM_POSIX, 1, [Define to 1 if you want 'posix' platform support.])
fi
,
enable_platform_posix=yes
enable_platform_corec=no
AC_DEFINE(ENABLE_PLATFORM_POSIX, 1, [Define to 1 if you want 'posix' platform support.])
)

AC_ARG_ENABLE([database],
[  --enable-database=[[database]] database can be one of [[sqlite3 (default),corec]]],
if test "$enableval" = "sqlite3"
then
	enable_database_sqlite3=yes
	enable_database_corec=no
	AC_DEFINE(ENABLE_DATABASE_SQLITE3, 1, [Define to 1 if you want 'sqlite3' platform support.])
elif test "$enableval" = "corec"
then
	enable_database_corec=yes
	enable_database_sqlite3=no
	AC_DEFINE(ENABLE_DATABASE_COREC, 1, [Define to 1 if you want 'corec' platform support.])
else
	enable_database_sqlite3=yes
	enable_database_corec=no
	AC_DEFINE(ENABLE_DATABASE_SQLITE3, 1, [Define to 1 if you want 'sqlite3' platform support.])
fi
,
enable_database_sqlite3=yes
enable_database_corec=no
AC_DEFINE(ENABLE_DATABASE_SQLITE3, 1, [Define to 1 if you want 'sqlite3' platform support.])
)

AC_ARG_ENABLE([xml],
[  --enable-parser=[[parser]] parser can be one of [[expat (default),corec]]],
if test "$enableval" = "sqlite3"
then
	enable_parser_expat=yes
	enable_parser_corec=no
	AC_DEFINE(ENABLE_PARSER_EXPAT, 1, [Define to 1 if you want 'expat' parser support.])
elif test "$enableval" = "corec"
then
	enable_parser_expat=yes
	enable_parser_sqlite3=no
	AC_DEFINE(ENABLE_PARSER_COREC, 1, [Define to 1 if you want 'corec' parser support.])
else
	enable_parser_expat=yes
	enable_parser_corec=no
	AC_DEFINE(ENABLE_PARSER_EXPAT, 1, [Define to 1 if you want 'expat' parser support.])
fi
,
enable_parser_expat=yes
enable_parser_corec=no
AC_DEFINE(ENABLE_PARSER_EXPAT, 1, [Define to 1 if you want 'expat' parser support.])
)

AC_ARG_ENABLE([controller],
[  --disable-controller    disable controller application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_controller=no
else
	enable_controller=yes
	AC_DEFINE(ENABLE_CONTROLLER, 1, [Define to 1 if you want 'controller' support.])
fi
,
enable_controller=yes
AC_DEFINE(ENABLE_CONTROLLER, 1, [Define to 1 if you want 'controller' support.])
)

AC_ARG_ENABLE([upnpfs],
[  --disable-upnpfs        disable upnpfs controller [[default=yes]]],
if test "$enableval" = "no" -o "$enable_libfuse" = "no" -o "$enable_controller" = "no"
then
	enable_upnpfs=no
else
	enable_upnpfs=yes
	AC_DEFINE(ENABLE_UPNPFS, 1, [Define to 1 if you want 'upnpfs' support.])
fi
,
if test "$enable_libfuse" = "yes" -a "$enable_controller" = "yes"
then
	enable_upnpfs=yes
	AC_DEFINE(ENABLE_UPNPFS, 1, [Define to 1 if you want 'upnpfs' support.])
fi
)

AC_ARG_ENABLE([mediaserver],
[  --disable-mediaserver   disable mediaserver application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_mediaserver=no
else
	enable_mediaserver=yes
	AC_DEFINE(ENABLE_MEDIASERVER, 1, [Define to 1 if you want 'mediaserver' support.])
fi
,
enable_mediaserver=yes
AC_DEFINE(ENABLE_MEDIASERVER, 1, [Define to 1 if you want 'mediaserver' support.])
)

AC_ARG_ENABLE([optional],
[  --enable-optional       enable optional upnp av functions [[default=no]]],
if test "$enableval" = "no"
then
	enable_optional=no
else
	enable_optional=yes
	AC_DEFINE(ENABLE_OPTIONAL, 1, [Define to 1 if you want 'optional' support.])
fi
,
enable_optional=no
)

#
# Variables to pass Makefiles
#

AM_CONDITIONAL(LINUX, test "$arch" = linux)
AM_CONDITIONAL(DARWIN, test "$arch" = darwin)

AM_CONDITIONAL(ENABLE_CONTROLLER, test "$enable_controller" = "yes")
AM_CONDITIONAL(ENABLE_MEDIASERVER, test "$enable_mediaserver" = "yes")
AM_CONDITIONAL(ENABLE_UPNPFS, test "$enable_upnpfs" = "yes")

AM_CONDITIONAL(ENABLE_LIBSDL, test "$enable_libsdl" = "yes")
AM_CONDITIONAL(ENABLE_LIBCAIRO, test "$enable_libcairo" = "yes")
AM_CONDITIONAL(ENABLE_LIBFFMPEG, test "$enable_libffmpeg" = "yes")

AM_CONDITIONAL(ENABLE_OPTIONAL, test "$enable_optional" = "yes")

#
# Create files
#

#AC_CONFIG_FILES([
#	Makefile
#	metadata/Makefile
#	upnpd/Makefile
#	upnpd/common/Makefile
#	upnpd/common/upnp/Makefile
#	upnpd/controller/Makefile
#	upnpd/mediaserver/Makefile
#	upnpfs/Makefile
#])

AC_OUTPUT

# coremake
echo "Building coremake tools/coremake/coremake.c"
gcc -O2 tools/coremake/coremake.c -o tools/coremake/coremake
if test "$arch" = "linux"
then
echo "Generating Makefiles (gcc_linux_debug)..."
tools/coremake/coremake -v gcc_linux_debug
else
if test "$arch" = "darwin"
then
echo "Generating Xcode Project (xcode_uni)..."
tools/coremake/coremake -v xcode_uni
fi
fi
