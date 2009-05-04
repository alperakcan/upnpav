
noinst_LIBRARIES = libupnp.a

libupnp_a_SOURCES = \
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
	upnp.h \
	ssdp.h \
	gena.h \
	gena.c \
	ssdp.c \
	upnp.c \
	debug.c

libupnp_a_CFLAGS = \
	-I$(top_srcdir)/ixml/inc \
	-I$(top_srcdir)/upnpd/common/upnp/uuid
