
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "metadata.h"

static char * metadata_type_string (metadata_type_t type)
{
	switch (type) {
		case METADATA_TYPE_CONTAINER: return "container";
		case METADATA_TYPE_AUDIO:     return "audio";
		case METADATA_TYPE_VIDEO:     return "video";
		case METADATA_TYPE_IMAGE:     return "image";
		default:                      return "unknown";
	}
}

int main (int argc, char *argv[])
{
	metadata_t *md;

	if (argc != 2) {
		printf("usage: %s mediafile\n", argv[0]);
		return -1;
	}

	md = upnpd_metadata_init(argv[1]);
	if (md == NULL) {
		printf("upnpd_metadata_init(%s) failed\n", argv[1]);
		return -1;
	}

	printf("%s\n", argv[1]);
	printf("  type: %s\n", metadata_type_string(md->type));
	printf("  pathname    : %s\n", md->pathname);
	printf("  basename    : %s\n", md->basename);
	printf("  mimetype    : %s\n", md->mimetype);
	printf("  dlnainfo    : %s\n", md->dlnainfo);
	printf("  date        : %s\n", md->date);
	printf("  title       : %s\n", md->title);
	printf("  artist      : %s\n", md->artist);
	printf("  album       : %s\n", md->album);
	printf("  genre       : %s\n", md->genre);
	printf("  duration    : %s\n", md->duration);
	printf("  size        : %llu\n", md->size);

	upnpd_metadata_uninit(md);

	return 0;
}
