
SUBDIRS     =
upnpd_LDADD =

bin_PROGRAMS =	\
	upnpd

upnpd_SOURCES =	\
	upnpd.c		\
	upnpd.h

upnpd_CFLAGS =	\
	-Wall	\
	-I$(top_srcdir)/upnpd/common \
	$(LIBUPNP_CFLAGS)

if ENABLE_CONTROLLER
SUBDIRS     += controller
upnpd_LDADD += $(top_builddir)/upnpd/controller/libcontroller.a
endif

if ENABLE_MEDIASERVER
SUBDIRS     += mediaserver
upnpd_LDADD += $(top_builddir)/upnpd/mediaserver/libmediaserver.a
endif

SUBDIRS     += common
upnpd_LDADD += \
	$(top_builddir)/upnpd/common/libcommon.a \
	$(top_builddir)/upnpd/common/upnp/libupnp.a \
	$(LIBUPNP_LIBS) \
	$(READLINE_LIBS) \
	-lrt
