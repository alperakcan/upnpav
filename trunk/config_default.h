/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* 1234 = LIL_ENDIAN, 4321 = BIGENDIAN */
#define BYTEORDER 1234

/* Define to 1 if you want 'controller' support. */
#define ENABLE_CONTROLLER 1

/* Define to 1 if you want 'corec' platform support. */
/* #undef ENABLE_DATABASE_COREC */

/* Define to 1 if you want 'sqlite3' platform support. */
#define ENABLE_DATABASE_SQLITE3 1

/* Define to 1 if you want 'mediaserver' support. */
#define ENABLE_MEDIASERVER 1

/* Define to 1 if you want 'optional' support. */
/* #undef ENABLE_OPTIONAL */

/* Define to 1 if you want 'corec' platform support. */
/* #undef ENABLE_PARSER_COREC */

/* Define to 1 if you want 'expat' platform support. */
#define ENABLE_PARSER_EXPAT 1

/* Define to 1 if you want 'corec' platform support. */
/* #undef ENABLE_PLATFORM_COREC */

/* Define to 1 if you want 'posix' platform support. */
#define ENABLE_PLATFORM_POSIX 1

/* Define to 1 if you want 'upnpfs' support. */
#define ENABLE_UPNPFS 1

/* Version of FUSE interface */
#define FUSE_USE_VERSION FUSE_VERSION

/* Define to 1 if you have the `bcopy' function. */
#define HAVE_BCOPY 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the <history.h> header file. */
/* #undef HAVE_HISTORY_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you want 'libffmpeg' support. */
/* #undef HAVE_LIBFFMPEG */

/* Define if you have a readline compatible library */
#define HAVE_LIBREADLINE 1

/* Define to 1 if your system has a GNU libc compatible `malloc' function, and
   to 0 otherwise. */
#define HAVE_MALLOC 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `printf' function. */
#define HAVE_PRINTF 1

/* Define to 1 if you have the <readline.h> header file. */
/* #undef HAVE_READLINE_H */

/* Define if your readline library has \`add_history' */
#define HAVE_READLINE_HISTORY 1

/* Define to 1 if you have the <readline/history.h> header file. */
#define HAVE_READLINE_HISTORY_H 1

/* Define to 1 if you have the <readline/readline.h> header file. */
#define HAVE_READLINE_READLINE_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcmp' function. */
#define HAVE_STRCMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "upnpd@alperakcan.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "upnpd"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "upnpd 0.0.2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "upnpd"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.0.2"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* whether byteorder is bigendian */
/* #undef WORDS_BIGENDIAN */

/* File Offset size */
#define _FILE_OFFSET_BITS 64

/* Large files support */
#define _LARGE_FILE_SOURCE /**/

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to rpl_malloc if the replacement function should be used. */
/* #undef malloc */

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
