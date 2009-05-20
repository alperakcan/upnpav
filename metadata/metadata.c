/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 - 2009 by Alper Akcan
    email                : alper.akcan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "metadata.h"

typedef struct metadata_info_s {
	metadata_type_t type;
	char *extension;
	char *mimetype;
	int (*metadata) (metadata_t *metadata);
} metadata_info_t;

static int metadata_video (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_VIDEO;
	metadata->title = strdup(metadata->basename);
	if (metadata->title == NULL) {
		return -1;
	}
	return 0;
}

static int metadata_audio (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_AUDIO;
	metadata->title = strdup(metadata->basename);
	if (metadata->title == NULL) {
		return -1;
	}
	return 0;
}

static int metadata_image (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_IMAGE;
	metadata->title = strdup(metadata->basename);
	if (metadata->title == NULL) {
		return -1;
	}
	return 0;
}

static metadata_info_t *metadata_info[] = {
	/* video */
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mpg", "video/mpeg", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mpeg", "video/mpeg", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.avi", "video/x-msvideo", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mts", "video/vnd.dlna.mpeg-tts", metadata_video},
	/* audio */
	& (metadata_info_t) {METADATA_TYPE_AUDIO, "*.mp3", "audio/mpeg", metadata_audio},
	& (metadata_info_t) {METADATA_TYPE_AUDIO, "*.ogg", "audio/ogg", metadata_audio},
	/* image */
	& (metadata_info_t) {METADATA_TYPE_IMAGE, "*.bmp", "image/bmp", metadata_image},
	& (metadata_info_t) {METADATA_TYPE_IMAGE, "*.jpg", "image/jpeg", metadata_image},
	& (metadata_info_t) {METADATA_TYPE_IMAGE, "*.jpeg", "image/jpeg", metadata_image},
	& (metadata_info_t) {METADATA_TYPE_IMAGE, "*.png", "image/png", metadata_image},
	& (metadata_info_t) {METADATA_TYPE_IMAGE, "*.tiff", "image/tiff", metadata_image},
	NULL,
};

metadata_t * metadata_init (const char *path)
{
	char *p;
	struct stat stbuf;
	metadata_t *m;
	metadata_info_t **info;

	if (access(path, R_OK) != 0) {
		return NULL;
	}
	if (stat(path, &stbuf) != 0) {
		return NULL;
	}

	m = NULL;
	if (S_ISREG(stbuf.st_mode)) {
		m = (metadata_t *) malloc(sizeof(metadata_t));
		if (m == NULL) {
			return NULL;
		}
		memset(m, 0, sizeof(metadata_t));

		m->type = METADATA_TYPE_UNKNOWN;
		p = strrchr(path, '/');
		m->pathname = strdup(path);
		m->basename = (p == NULL) ? strdup(path) : strdup(p + 1);
		m->size = stbuf.st_size;
		for (info = metadata_info; *info != NULL; info++) {
			if (fnmatch((*info)->extension, m->basename, FNM_CASEFOLD) == 0) {
				m->type = (*info)->type;
				m->mimetype = strdup((*info)->mimetype);
				break;
			}
		}
		if (m->type == METADATA_TYPE_UNKNOWN ||
		    m->pathname == NULL ||
		    m->basename == NULL ||
		    m->mimetype == NULL) {
			goto error;
		}
		if ((*info)->metadata(m) != 0) {
			goto error;
		}
		return m;
	} else if (S_ISDIR(stbuf.st_mode)) {
		m = (metadata_t *) malloc(sizeof(metadata_t));
		if (m == NULL) {
			return NULL;
		}
		memset(m, 0, sizeof(metadata_t));

		m->type = METADATA_TYPE_CONTAINER;
		p = strrchr(path, '/');
		m->pathname = strdup(path);
		m->basename = (p == NULL) ? strdup(path) : strdup(p + 1);
		m->size = stbuf.st_size;
		if (m->pathname == NULL ||
		    m->basename == NULL) {
			goto error;
		}
		return m;
	}
error:
	metadata_uninit(m);
	return NULL;
}

int metadata_uninit (metadata_t *metadata)
{
	if (metadata == NULL) {
		return 0;
	}
	free(metadata->pathname);
	free(metadata->basename);
	free(metadata->mimetype);
	free(metadata->dlnainfo);
	free(metadata->title);
	free(metadata->artist);
	free(metadata->album);
	free(metadata->genre);
	free(metadata->duration);
	free(metadata->image);
	free(metadata);
	return 0;
}
