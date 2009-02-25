
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

dnl @synopsis ACX_PTHREAD([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
dnl
dnl @summary figure out how to build C programs using POSIX threads
dnl
dnl This macro figures out how to build C programs using POSIX threads.
dnl It sets the PTHREAD_LIBS output variable to the threads library and
dnl linker flags, and the PTHREAD_CFLAGS output variable to any special
dnl C compiler flags that are needed. (The user can also force certain
dnl compiler flags/libs to be tested by setting these environment
dnl variables.)
dnl
dnl Also sets PTHREAD_CC to any special C compiler that is needed for
dnl multi-threaded programs (defaults to the value of CC otherwise).
dnl (This is necessary on AIX to use the special cc_r compiler alias.)
dnl
dnl NOTE: You are assumed to not only compile your program with these
dnl flags, but also link it with them as well. e.g. you should link
dnl with $PTHREAD_CC $CFLAGS $PTHREAD_CFLAGS $LDFLAGS ... $PTHREAD_LIBS
dnl $LIBS
dnl
dnl If you are only building threads programs, you may wish to use
dnl these variables in your default LIBS, CFLAGS, and CC:
dnl
dnl        LIBS="$PTHREAD_LIBS $LIBS"
dnl        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
dnl        CC="$PTHREAD_CC"
dnl
dnl In addition, if the PTHREAD_CREATE_JOINABLE thread-attribute
dnl constant has a nonstandard name, defines PTHREAD_CREATE_JOINABLE to
dnl that name (e.g. PTHREAD_CREATE_UNDETACHED on AIX).
dnl
dnl ACTION-IF-FOUND is a list of shell commands to run if a threads
dnl library is found, and ACTION-IF-NOT-FOUND is a list of commands to
dnl run it if it is not found. If ACTION-IF-FOUND is not specified, the
dnl default action will define HAVE_PTHREAD.
dnl
dnl Please let the authors know if this macro fails on any platform, or
dnl if you have any other suggestions or comments. This macro was based
dnl on work by SGJ on autoconf scripts for FFTW (www.fftw.org) (with
dnl help from M. Frigo), as well as ac_pthread and hb_pthread macros
dnl posted by Alejandro Forero Cuervo to the autoconf macro repository.
dnl We are also grateful for the helpful feedback of numerous users.
dnl
dnl @category InstalledPackages
dnl @author Steven G. Johnson <stevenj@alum.mit.edu>
dnl @version 2005-06-15
dnl @license GPLWithACException

AC_DEFUN([ACX_PTHREAD], [
AC_REQUIRE([AC_CANONICAL_HOST])
AC_LANG_SAVE
AC_LANG_C
acx_pthread_ok=no

# We used to check for pthread.h first, but this fails if pthread.h
# requires special compiler flags (e.g. on True64 or Sequent).
# It gets checked for in the link test anyway.

# First of all, check if the user has set any of the PTHREAD_LIBS,
# etcetera environment variables, and if threads linking works using
# them:
if test x"$PTHREAD_LIBS$PTHREAD_CFLAGS" != x; then
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        AC_MSG_CHECKING([for pthread_join in LIBS=$PTHREAD_LIBS with CFLAGS=$PTHREAD_CFLAGS])
        AC_TRY_LINK_FUNC(pthread_join, acx_pthread_ok=yes)
        AC_MSG_RESULT($acx_pthread_ok)
        if test x"$acx_pthread_ok" = xno; then
                PTHREAD_LIBS=""
                PTHREAD_CFLAGS=""
        fi
        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"
fi

# We must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. DEC) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-POSIX).

# Create a list of thread flags to try.  Items starting with a "-" are
# C compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all, and "pthread-config"
# which is a program returning the flags for the Pth emulation library.

acx_pthread_flags="pthreads none -Kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt pthread-config"

# The ordering *is* (sometimes) important.  Some notes on the
# individual items follow:

# pthreads: AIX (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -Kthread and
#       other compiler flags to prevent continual compiler warnings
# -Kthread: Sequent (threads in libc, but -Kthread needed for pthread.h)
# -kthread: FreeBSD kernel threads (preferred to -pthread since SMP-able)
# lthread: LinuxThreads port on FreeBSD (also preferred to -pthread)
# -pthread: Linux/gcc (kernel threads), BSD/gcc (userland threads)
# -pthreads: Solaris/gcc
# -mthreads: Mingw32/gcc, Lynx/gcc
# -mt: Sun Workshop C (may only link SunOS threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -D_REENTRANT)
#      ... -mt is also the pthreads flag for HP/aCC
# pthread: Linux, etcetera
# --thread-safe: KAI C++
# pthread-config: use pthread-config program (for GNU Pth library)

case "${host_cpu}-${host_os}" in
        *solaris*)

        # On Solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (We need to link with -pthreads/-mt/
        # -lpthread.)  (The stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  So,
        # we'll just look for -pthreads and -lpthread first:

        acx_pthread_flags="-pthreads pthread -mt -pthread $acx_pthread_flags"
        ;;
esac

if test x"$acx_pthread_ok" = xno; then
for flag in $acx_pthread_flags; do

        case $flag in
                none)
                AC_MSG_CHECKING([whether pthreads work without any flags])
                ;;

                -*)
                AC_MSG_CHECKING([whether pthreads work with $flag])
                PTHREAD_CFLAGS="$flag"
                ;;

		pthread-config)
		AC_CHECK_PROG(acx_pthread_config, pthread-config, yes, no)
		if test x"$acx_pthread_config" = xno; then continue; fi
		PTHREAD_CFLAGS="`pthread-config --cflags`"
		PTHREAD_LIBS="`pthread-config --ldflags` `pthread-config --libs`"
		;;

                *)
                AC_MSG_CHECKING([for the pthreads library -l$flag])
                PTHREAD_LIBS="-l$flag"
                ;;
        esac

        save_LIBS="$LIBS"
        save_CFLAGS="$CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Check for various functions.  We must include pthread.h,
        # since some functions may be macros.  (On the Sequent, we
        # need a special flag -Kthread to make this header compile.)
        # We check for pthread_join because it is in -lpthread on IRIX
        # while pthread_create is in libc.  We check for pthread_attr_init
        # due to DEC craziness with -lpthreads.  We check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on Solaris that doesn't have a non-functional libc stub.
        # We try pthread_create on general principles.
        AC_TRY_LINK([#include <pthread.h>],
                    [pthread_t th; pthread_join(th, 0);
                     pthread_attr_init(0); pthread_cleanup_push(0, 0);
                     pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
                    [acx_pthread_ok=yes])

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        AC_MSG_RESULT($acx_pthread_ok)
        if test "x$acx_pthread_ok" = xyes; then
                break;
        fi

        PTHREAD_LIBS=""
        PTHREAD_CFLAGS=""
done
fi

# Various other checks:
if test "x$acx_pthread_ok" = xyes; then
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Detect AIX lossage: JOINABLE attribute is called UNDETACHED.
	AC_MSG_CHECKING([for joinable pthread attribute])
	attr_name=unknown
	for attr in PTHREAD_CREATE_JOINABLE PTHREAD_CREATE_UNDETACHED; do
	    AC_TRY_LINK([#include <pthread.h>], [int attr=$attr; return attr;],
                        [attr_name=$attr; break])
	done
        AC_MSG_RESULT($attr_name)
        if test "$attr_name" != PTHREAD_CREATE_JOINABLE; then
            AC_DEFINE_UNQUOTED(PTHREAD_CREATE_JOINABLE, $attr_name,
                               [Define to necessary symbol if this constant
                                uses a non-standard name on your system.])
        fi

        AC_MSG_CHECKING([if more special flags are required for pthreads])
        flag=no
        case "${host_cpu}-${host_os}" in
            *-aix* | *-freebsd* | *-darwin*) flag="-D_THREAD_SAFE";;
            *solaris* | *-osf* | *-hpux*) flag="-D_REENTRANT";;
        esac
        AC_MSG_RESULT(${flag})
        if test "x$flag" != xno; then
            PTHREAD_CFLAGS="$flag $PTHREAD_CFLAGS"
        fi

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        # More AIX lossage: must compile with cc_r
        AC_CHECK_PROG(PTHREAD_CC, cc_r, cc_r, ${CC})
else
        PTHREAD_CC="$CC"
fi

AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_CC)

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x"$acx_pthread_ok" = xyes; then
        ifelse([$1],,AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.]),[$1])
        :
else
        acx_pthread_ok=no
        $2
fi
AC_LANG_RESTORE
])dnl ACX_PTHREAD

#
# Check for libraries
#

#
# readline
#

VL_LIB_READLINE

#
# POSIX Threads
#

ACX_PTHREAD([],[AC_MSG_ERROR([POSIX threads are required to build this program])])

LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

# - libmagic

AC_CHECK_HEADERS([magic.h], [AC_SUBST([MAGIC_CFLAGS], [-DHAVE_MAGIC=1])], [])
AC_CHECK_LIB([magic], [magic_open], [AC_SUBST([MAGIC_LIBS], [-lmagic])], [], [-lz])
LIBS="$LIBS $MAGIC_LIBS"
CFLAGS="$CFLAGS $MAGIC_CFLAGS"

#
# Check extra parameters
#

AC_ARG_ENABLE([test_render],
[  --disable-test_render   disable test_render application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_render=no
	echo "Disabling test_render application"
else
	enable_test_render=yes
	AC_DEFINE(ENABLE_TESTRENDER, 1, [Define to 1 if you want 'test_render' support.])
	echo "Enabling test_render application"
fi
,
enable_test_render=yes
echo "Enabling test_render application by default"
AC_DEFINE(ENABLE_TESTRENDER, 1, [Define to 1 if you want 'test_render' support.])
)

AC_ARG_ENABLE([test_browse],
[  --disable-test_browse   disable test_browse application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_browse=no
	echo "Disabling test_browse application"
else
	enable_test_browse=yes
	AC_DEFINE(ENABLE_TESTBROWSE, 1, [Define to 1 if you want 'test_browse' support.])
	echo "Enabling test_browse application"
fi
,
enable_test_browse=yes
echo "Enabling test_browse application by default"
AC_DEFINE(ENABLE_TESTBROWSE, 1, [Define to 1 if you want 'test_browse' support.])
)

AC_ARG_ENABLE([test_client],
[  --disable-test_client   disable test_client application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_client=no
	echo "Disabling test_client application"
else
	enable_test_client=yes
	AC_DEFINE(ENABLE_TESTCLIENT, 1, [Define to 1 if you want 'test_client' support.])
	echo "Enabling test_client application"
fi
,
enable_test_client=yes
echo "Enabling test_client application by default"
AC_DEFINE(ENABLE_TESTCLIENT, 1, [Define to 1 if you want 'test_client' support.])
)

AC_ARG_ENABLE([test_device],
[  --disable-test_device   disable test_device application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_test_device=no
	echo "Disabling test_device application"
else
	enable_test_device=yes
	AC_DEFINE(ENABLE_TESTDEVICE, 1, [Define to 1 if you want 'test_device' support.])
	echo "Enabling test_device application"
fi
,
enable_test_device=yes
echo "Enabling test_device application by default"
AC_DEFINE(ENABLE_TESTDEVICE, 1, [Define to 1 if you want 'test_device' support.])
)

AC_ARG_ENABLE([optional],
[  --enable-optional       enable optional upnp av functions [[default=no]]],
if test "$enableval" = "no"
then
	enable_optional=no
	echo "Disabling optional functions"
else
	enable_optional=yes
	AC_DEFINE(ENABLE_OPTIONAL, 1, [Define to 1 if you want 'optional' support.])
	echo "Enabling optional functions"
fi
,
enable_optional=no
echo "Disabling optional functions by default"
)

AC_ARG_ENABLE([controller],
[  --disable-controller    disable controller application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_controller=no
	echo "Disabling controller application"
else
	enable_controller=yes
	AC_DEFINE(ENABLE_CONTROLLER, 1, [Define to 1 if you want 'controller' support.])
	echo "Enabling controller application"
fi
,
enable_controller=yes
echo "Enabling controller application by default"
AC_DEFINE(ENABLE_CONTROLLER, 1, [Define to 1 if you want 'controller' support.])
)

AC_ARG_ENABLE([mediaserver],
[  --disable-mediaserver   disable mediaserver application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_mediaserver=no
	echo "Disabling mediaserver application"
else
	enable_mediaserver=yes
	AC_DEFINE(ENABLE_MEDIASERVER, 1, [Define to 1 if you want 'mediaserver' support.])
	echo "Enabling mediaserver application"
fi
,
enable_mediaserver=yes
echo "Enabling mediaserver application by default"
AC_DEFINE(ENABLE_MEDIASERVER, 1, [Define to 1 if you want 'mediaserver' support.])
)

AC_ARG_ENABLE([mediarender],
[  --disable-mediarender   disable mediarender application [[default=yes]]],
if test "$enableval" = "no"
then
	enable_mediarender=no
	echo "Disabling mediarender application"
else
	enable_mediarender=yes
	AC_DEFINE(ENABLE_MEDIARENDER, 1, [Define to 1 if you want 'mediarender' support.])
	echo "Enabling mediarender application"
fi
,
enable_mediarender=yes
echo "Enabling mediarender application by default"
AC_DEFINE(ENABLE_MEDIARENDER, 1, [Define to 1 if you want 'mediarender' support.])
)

# - libpupnp

PKG_CHECK_MODULES(LIBUPNP, libupnp, HAVE_LINUPNP=yes, HAVE_LINUPNP=no)
if test "$HAVE_LINUPNP" = "yes"
then
	enable_libupnp=yes
	AC_DEFINE(ENABLE_LINUPNP, 1, [Define to 1 if you want 'libupnp' support.])
	echo "Enabling LibUPnP support"
else
	enable_libupnp=no
	echo "Disabling LibUPnP support"
	AC_MSG_ERROR(Can't find libupnp)
fi

# - xynth

PKG_CHECK_MODULES(XYNTH, xynth, HAVE_XYNTH=yes, HAVE_XYNTH=no)
if test "$HAVE_XYNTH" = "yes"
then
	enable_xynth=yes
	AC_DEFINE(ENABLE_XYNTH, 1, [Define to 1 if you want 'xynth' support.])
	echo "Enabling Xynth Graphical User Interface support"
else
	enable_xynth=no
	echo "Disabling Xynth Graphical User Interface support"
fi

# - cairo

PKG_CHECK_MODULES(CAIRO, cairo, HAVE_CAIRO=yes, HAVE_CAIRO=no)
if test "$HAVE_CAIRO" = "yes"
then
	enable_cairo=yes
	AC_DEFINE(ENABLE_CAIRO, 1, [Define to 1 if you want 'cairo' support.])
	echo "Enabling Cairo Graphical User Interface support"
else
	enable_cairo=no
	echo "Disabling CAIRO Graphical User Interface support"
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
	echo "Disabling GStreamer rendering support"
else
	if test "$HAVE_GSTREAMER" = "yes"
	then
		enable_render_gstreamer=yes
		AC_DEFINE(ENABLE_RENDER_GSTREAMER, 1, [Define to 1 if you want 'gstreamer rendering' support.])
		echo "Enabling GStreamer rendering support"
	else
		enable_render_gstreamer=no
		echo "Disabling GStreamer rendering support"
	fi
fi
,
if test "$HAVE_GSTREAMER" = "yes" -a "$enable_mediarender" = "yes"
then
	enable_render_gstreamer=yes
	AC_DEFINE(ENABLE_RENDER_GSTREAMER, 1, [Define to 1 if you want 'gstreamer rendering' support.])
	echo "Enabling GStreamer rendering support"
else
	enable_render_gstreamer=no
	echo "Disabling GStreamer rendering support"
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
	echo "Disabling FFmpeg rendering support"
else
	if test "$HAVE_FFMPEG" = "yes"
	then
		enable_render_ffmpeg=yes
		AC_DEFINE(ENABLE_RENDER_FFMPEG, 1, [Define to 1 if you want 'ffmpeg rendering' support.])
		echo "Enabling FFmpeg rendering support"
	else
		enable_render_ffmpeg=no
		echo "Disabling FFmpeg rendering support"
	fi
fi
,
if test "$HAVE_FFMPEG" = "yes" -a "$enable_mediarender" = "yes"
then
	enable_render_ffmpeg=yes
	AC_DEFINE(ENABLE_RENDER_FFMPEG, 1, [Define to 1 if you want 'ffmpeg rendering' support.])
	echo "Enabling FFmpeg rendering support"
else
	enable_render_ffmpeg=no
	echo "Disabling FFmpeg rendering support"
fi
)

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
AM_CONDITIONAL(ENABLE_CAIRO, test "$enable_cairo" = "yes")
AM_CONDITIONAL(ENABLE_XYNTH, test "$enable_xynth" = "yes")
AM_CONDITIONAL(ENABLE_RENDER_GSTREAMER, test "$enable_render_gstreamer" = "yes")
AM_CONDITIONAL(ENABLE_RENDER_FFMPEG, test "$enable_render_ffmpeg" = "yes")
AM_CONDITIONAL(ENABLE_TESTRENDER, test "$enable_test_render" = "yes")
AM_CONDITIONAL(ENABLE_TESTBROWSE, test "$enable_test_browse" = "yes")
AM_CONDITIONAL(ENABLE_TESTCLIENT, test "$enable_test_client" = "yes")
AM_CONDITIONAL(ENABLE_TESTDEVICE, test "$enable_test_device" = "yes")
AM_CONDITIONAL(ENABLE_OPTIONAL, test "$enable_optional" = "yes")
AM_CONDITIONAL(ENABLE_CONTROLLER, test "$enable_controller" = "yes")
AM_CONDITIONAL(ENABLE_MEDIASERVER, test "$enable_mediaserver" = "yes")
AM_CONDITIONAL(ENABLE_MEDIARENDER, test "$enable_mediarender" = "yes")

#
# Create files
#

AC_CONFIG_FILES([
	Makefile
	Documentation/Makefile
	upnpd/Makefile
	upnpd/common/Makefile
	upnpd/controller/Makefile
	upnpd/mediaserver/Makefile
	upnpd/mediarender/Makefile
])

AC_OUTPUT
