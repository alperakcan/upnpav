
SUBDIRS     =
upnpd_LDADD =

bin_PROGRAMS =	\
	upnpd

upnpd_SOURCES =	\
	upnpd.c		\
	upnpd.h

upnpd_CFLAGS =	\
	-Wall	\
	-I$(top_srcdir)/ixml/inc \
	-I$(top_srcdir)/upnpd \
	-I$(top_srcdir)/upnpd/common \
	-I$(top_srcdir)/upnpd/common/upnp

if ENABLE_CONTROLLER
SUBDIRS      += controller
upnpd_CFLAGS += $(LIBUPNP_CFLAGS)
upnpd_LDADD  += $(top_builddir)/upnpd/controller/libcontroller.a
endif

if ENABLE_MEDIASERVER
SUBDIRS     += mediaserver
upnpd_LDADD += $(top_builddir)/upnpd/mediaserver/libmediaserver.a
endif

SUBDIRS     += common
upnpd_LDADD += \
	$(top_builddir)/upnpd/common/libcommon.a \
	$(top_builddir)/upnpd/common/upnp/libupnp.a \
	$(top_builddir)/ixml/libixml.a \
	$(READLINE_LIBS) \
	-lpthread

if ENABLE_CONTROLLER
upnpd_LDADD += $(LIBUPNP_LIBS) -lrt
endif
