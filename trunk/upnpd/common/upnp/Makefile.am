
noinst_PROGRAMS  = test_ssdp
noinst_LIBRARIES = libupnp.a

libupnp_a_SOURCES = \
	upnp.h \
	gena.c \
	ssdp.c \
	upnp.c

test_ssdp_SOURCES = \
	test_ssdp.c
test_ssdp_LDADD   = \
	$(top_builddir)/upnpd/common/upnp/libupnp.a $(LIBUPNP_LIBS)
