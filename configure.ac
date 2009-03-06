
AC_INIT([upnpd], [0.0.1], [upnpd@alperakcan.org])
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADER([config.h])

# Initialize config.h
AC_GNU_SOURCE

# Enable maintainer mode
AM_MAINTAINER_MODE

# Disable shared
AC_DISABLE_SHARED

# Enable large files (off_t is 64bits or more)
#AC_SYS_LARGEFILE

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

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_CHECK_HEADERS([	\
	stdio.h		\
	stdlib.h	\
	string.h	\
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([	\
	printf		\
	strcmp		\
	memset		\
])

#
# M4 Commands
#

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

])dnl

CFLAGS="$CFLAGS -Wall -Werror"

#
# Check extra parameters
#

AC_ARG_ENABLE([test_render],
[  --enable-test_render   disable test_render application [[default=no]]],
if test "$enableval" = "no"
then
	enable_test_render=no
else
	enable_test_render=yes
	AC_DEFINE(ENABLE_TESTRENDER, 1, [Define to 1 if you want 'test_render' support.])
fi
,
enable_test_render=no
)

AC_ARG_ENABLE([test_browse],
[  --disable-test_browse   disable test_browse application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_browse=no
else
	enable_test_browse=yes
	AC_DEFINE(ENABLE_TESTBROWSE, 1, [Define to 1 if you want 'test_browse' support.])
fi
,
enable_test_browse=yes
AC_DEFINE(ENABLE_TESTBROWSE, 1, [Define to 1 if you want 'test_browse' support.])
)

AC_ARG_ENABLE([test_client],
[  --disable-test_client   disable test_client application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_client=no
else
	enable_test_client=yes
	AC_DEFINE(ENABLE_TESTCLIENT, 1, [Define to 1 if you want 'test_client' support.])
fi
,
enable_test_client=yes
AC_DEFINE(ENABLE_TESTCLIENT, 1, [Define to 1 if you want 'test_client' support.])
)

AC_ARG_ENABLE([test_device],
[  --disable-test_device   disable test_device application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_device=no
else
	enable_test_device=yes
	AC_DEFINE(ENABLE_TESTDEVICE, 1, [Define to 1 if you want 'test_device' support.])
fi
,
enable_test_device=yes
AC_DEFINE(ENABLE_TESTDEVICE, 1, [Define to 1 if you want 'test_device' support.])
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

AC_ARG_ENABLE([mediarender],
[  --enable-mediarender   disable mediarender application [[default=no]]],
if test "$enableval" = "no"
then
	enable_mediarender=no
else
	enable_mediarender=yes
	AC_DEFINE(ENABLE_MEDIARENDER, 1, [Define to 1 if you want 'mediarender' support.])
fi
,
enable_mediarender=no
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
# dependencies
#

# - readline

VL_LIB_READLINE

# - libpupnp

PKG_CHECK_MODULES(LIBUPNP, libupnp, HAVE_LINUPNP=yes, HAVE_LINUPNP=no)
if test "$HAVE_LINUPNP" = "yes"
then
	enable_libupnp=yes
	AC_DEFINE(ENABLE_LINUPNP, 1, [Define to 1 if you want 'libupnp' support.])
else
	enable_libupnp=no
	AC_MSG_ERROR(Can't find libupnp)
fi

# - gstreamer

GST_REQS=0.10.1
GSTPLUG_REQS=0.10.1
GST_MAJORMINOR=0.10
PKG_CHECK_MODULES(GST, gstreamer-$GST_MAJORMINOR >= $GST_REQS, HAVE_GSTREAMER=yes, HAVE_GSTREAMER=no)

enable_render_gstreamer=no

AC_ARG_ENABLE([gstreamer],
[  --disable-gstreamer     disable gstreamer render support [[default=yes]]],
if test "$enableval" = "no"
then
	enable_render_gstreamer=no
else
	if test "$HAVE_GSTREAMER" = "yes"
	then
		enable_render_gstreamer=yes
		AC_DEFINE(ENABLE_RENDER_GSTREAMER, 1, [Define to 1 if you want 'gstreamer rendering' support.])
	else
		enable_render_gstreamer=no
	fi
fi
,
if test "$HAVE_GSTREAMER" = "yes" -a "$enable_mediarender" = "yes"
then
	enable_render_gstreamer=yes
	AC_DEFINE(ENABLE_RENDER_GSTREAMER, 1, [Define to 1 if you want 'gstreamer rendering' support.])
else
	enable_render_gstreamer=no
fi
)

# - ffmpeg

PKG_CHECK_MODULES(FFMPEG, libavdevice libavcodec libavformat libswscale libavutil sdl alsa, HAVE_FFMPEG=yes, HAVE_FFMPEG=no)
AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)

enable_render_ffmpeg=no

AC_ARG_ENABLE([ffmpeg],
[  --disable-ffmpeg        disable ffmpeg render support [[default=yes]]],
if test "$enableval" = "no"
then
	enable_render_ffmpeg=no
else
	if test "$HAVE_FFMPEG" = "yes"
	then
		enable_render_ffmpeg=yes
		AC_DEFINE(ENABLE_RENDER_FFMPEG, 1, [Define to 1 if you want 'ffmpeg rendering' support.])
	else
		enable_render_ffmpeg=no
	fi
fi
,
if test "$HAVE_FFMPEG" = "yes" -a "$enable_mediarender" = "yes"
then
	enable_render_ffmpeg=yes
	AC_DEFINE(ENABLE_RENDER_FFMPEG, 1, [Define to 1 if you want 'ffmpeg rendering' support.])
else
	enable_render_ffmpeg=no
fi
)

# - libmagic

AC_CHECK_HEADERS([magic.h], [AC_SUBST([MAGIC_CFLAGS], [-DHAVE_MAGIC=1])], [])
AC_CHECK_LIB([magic], [magic_open], [AC_SUBST([MAGIC_LIBS], [-lmagic])], [], [-lz])
LIBS="$LIBS $MAGIC_LIBS"
CFLAGS="$CFLAGS $MAGIC_CFLAGS"

AC_ARG_WITH(pkgconfigdir,
            [  --with-pkgconfigdir=DIR pkgconfig file in DIR @<:@LIBDIR/pkgconfig@:>@],
            [pkgconfigdir=$withval],
            [pkgconfigdir='${libdir}/pkgconfig'])
AC_SUBST(pkgconfigdir)

#
# Variables to pass Makefiles
#

AM_CONDITIONAL(LINUX, test "$arch" = linux)
AM_CONDITIONAL(DARWIN, test "$arch" = darwin)

AM_CONDITIONAL(ENABLE_RENDER_GSTREAMER, test "$enable_render_gstreamer" = "yes")
AM_CONDITIONAL(ENABLE_RENDER_FFMPEG, test "$enable_render_ffmpeg" = "yes")

AM_CONDITIONAL(ENABLE_TESTRENDER, test "$enable_test_render" = "yes")
AM_CONDITIONAL(ENABLE_TESTBROWSE, test "$enable_test_browse" = "yes")
AM_CONDITIONAL(ENABLE_TESTCLIENT, test "$enable_test_client" = "yes")
AM_CONDITIONAL(ENABLE_TESTDEVICE, test "$enable_test_device" = "yes")

AM_CONDITIONAL(ENABLE_CONTROLLER, test "$enable_controller" = "yes")
AM_CONDITIONAL(ENABLE_MEDIASERVER, test "$enable_mediaserver" = "yes")
AM_CONDITIONAL(ENABLE_MEDIARENDER, test "$enable_mediarender" = "yes")

AM_CONDITIONAL(ENABLE_OPTIONAL, test "$enable_optional" = "yes")

#
# Create files
#

AC_CONFIG_FILES([
	Makefile
	Documentation/Makefile
	upnpd/Makefile
	upnpd/common/Makefile
	upnpd/common/upnp/Makefile
	upnpd/controller/Makefile
	upnpd/mediaserver/Makefile
	upnpd/mediarender/Makefile
])

AC_OUTPUT
