
noinst_PROGRAMS  = metadata
noinst_LIBRARIES = libmetadata.a

libmetadata_a_SOURCES = \
	metadata.h \
	snapshot.c

libmetadata_a_CFLAGS =	\
	$(LIBFFMPEG_CFLAGS)

metadata_SOURCES = \
	metadata.c

metadata_LDADD = \
	$(LIBFFMPEG_LIBS)
