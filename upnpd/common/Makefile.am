
noinst_PROGRAMS  =
noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	upnp/ssdp.c \
	uuid/clear.c \
	uuid/compare.c \
	uuid/copy.c \
	uuid/gen_uuid.c \
	uuid/isnull.c \
	uuid/pack.c \
	uuid/parse.c \
	uuid/unpack.c \
	uuid/unparse.c \
	uuid/uuid_time.c \
	uuid/uuid.h \
	uuid/uuidP.h \
	client.c \
	entry.c \
	debug.c \
	device.c \
	upnp.c \
	connection.c \
	contentdir.c \
	transport.c \
	rendering.c \
	service.c \
	inttool.c \
	xml.c \
	uri.c \
	webserver.c \
	render.c \
	interface.c \
	common.h

libcommon_a_CFLAGS =	\
	-Wall	\
	$(LIBUPNP_CFLAGS)

if ENABLE_MEDIASERVER
libcommon_a_SOURCES += mediaserver.c
endif

if ENABLE_MEDIARENDER
libcommon_a_SOURCES += mediarender.c
endif

if ENABLE_CONTROLLER
libcommon_a_SOURCES += controller.c
endif

if ENABLE_RENDER_GSTREAMER
libcommon_a_SOURCES += render_gstreamer.c
libcommon_a_CFLAGS  += $(GST_CFLAGS)
endif

if ENABLE_RENDER_FFMPEG
libcommon_a_SOURCES += render_ffmpeg.c
libcommon_a_CFLAGS  += $(FFMPEG_CFLAGS) -DFFPLAY_UPNPD
endif

if ENABLE_TESTCLIENT
noinst_PROGRAMS    += test_client
test_client_SOURCES = test_client.c
test_client_CFLAGS  = -Wall $(LIBUPNP_CFLAGS)
test_client_LDADD   = $(top_builddir)/upnpd/common/libcommon.a $(LIBUPNP_LIBS) $(GST_LIBS)
endif

if ENABLE_TESTDEVICE
noinst_PROGRAMS    += test_device
test_device_SOURCES = test_device.c
test_device_CFLAGS  = -Wall $(LIBUPNP_CFLAGS)
test_device_LDADD   = $(top_builddir)/upnpd/common/libcommon.a $(LIBUPNP_LIBS) $(GST_LIBS)
endif

if ENABLE_TESTBROWSE
noinst_PROGRAMS    += test_browse
test_browse_SOURCES = test_browse.c
test_browse_CFLAGS  = -Wall $(LIBUPNP_CFLAGS)
test_browse_LDADD   = $(top_builddir)/upnpd/common/libcommon.a $(LIBUPNP_LIBS) $(LIBMAGIC) $(GST_LIBS)
endif

if ENABLE_TESTRENDER
noinst_PROGRAMS    += test_render
test_render_SOURCES = test_render.c
test_render_CFLAGS  = -Wall $(LIBUPNP_CFLAGS)
test_render_LDADD   = $(top_builddir)/upnpd/common/libcommon.a $(LIBUPNP_LIBS) $(LIB_MAGIC) $(GST_LIBS) $(FFMPEG_LIBS) -lbz2
endif
