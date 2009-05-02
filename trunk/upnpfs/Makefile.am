
bin_PROGRAMS = \
	upnpfs

upnpfs_SOURCES = \
	upnpfs.h \
	upnpfs.c \
	do_findpath.c \
	op_access.c \
	op_init.c \
	op_destroy.c \
	op_opendir.c \
	op_readdir.c \
	op_getattr.c \
	op_open.c \
	op_read.c \
	op_release.c

upnpfs_CFLAGS = \
	-D_FILE_OFFSET_BITS=64 \
	-I$(top_srcdir)/ixml/inc \
	-I$(top_srcdir)/upnpd \
	-I$(top_srcdir)/upnpd/common \
	-I$(top_srcdir)/upnpd/common/upnp

if DARWIN
upnpfs_CFLAGS += \
	-D__FreeBSD__=10
endif

upnpfs_LDADD = \
	$(top_builddir)/upnpd/common/libcommon.a \
	$(top_builddir)/upnpd/common/upnp/libupnp.a \
	$(top_builddir)/ixml/libixml.a \
	-lfuse
