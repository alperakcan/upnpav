
noinst_LIBRARIES = libcontroller.a

libcontroller_a_SOURCES = \
	controller.c \
	console.c

if ENABLE_LIBSDL
libcontroller_a_SOURCES += \
	sdl.c \
	sdl_browser.c \
	sdl_setup.c \
	sdl.h
endif

libcontroller_a_CFLAGS = \
	-I$(top_srcdir)/metadata \
	-I$(top_srcdir)/ixml/inc \
	-I$(top_srcdir)/upnpd \
	-I$(top_srcdir)/upnpd/common \
	-I$(top_srcdir)/upnpd/common/upnp \
	$(LIBSDL_CFLAGS) \
	$(LIBCAIRO_CFLAGS)
