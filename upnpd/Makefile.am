
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

if ENABLE_MEDIARENDER
SUBDIRS     += mediarender
upnpd_LDADD += \
	$(top_builddir)/upnpd/mediarender/libmediarender.a \
	-lbz2
endif

SUBDIRS     += common
upnpd_LDADD += \
	$(top_builddir)/upnpd/common/libcommon.a \
	$(LIBUPNP_LIBS) \
	$(READLINE_LIBS) \
	-lrt

#
# external stuff must be after all, not to confuse ld library path
#

if ENABLE_RENDER_GSTREAMER
upnpd_LDADD += $(GST_LIBS)
endif

if ENABLE_RENDER_FFMPEG
upnpd_LDADD += $(FFMPEG_LIBS)
endif
