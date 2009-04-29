
SUBDIRS = upnp

noinst_PROGRAMS  =
noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
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
	entry.c \
	debug.c \
	device.c \
	upnp.c \
	connection.c \
	contentdir.c \
	service.c \
	inttool.c \
	xml.c \
	uri.c \
	webserver.c \
	interface.c \
	common.h

libcommon_a_CFLAGS =	\
	-Wall \
	-I$(top_srcdir)/ixml/inc \
	-I$(top_srcdir)/upnpd \
	-I$(top_srcdir)/upnpd/common \
	-I$(top_srcdir)/upnpd/common/upnp \
	-I$(top_srcdir)/upnpd/common/uuid

if ENABLE_MEDIASERVER
libcommon_a_SOURCES += mediaserver.c
endif

if ENABLE_CONTROLLER
libcommon_a_SOURCES += \
	client.c \
	controller.c
endif
