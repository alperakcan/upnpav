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

static int metadata_file_is_audio (const char *path)
{
	if (fnmatch("*.mp3", path, FNM_CASEFOLD) == 0) {
		return 0;
	}
	return -1;
}

static int metadata_file_is_video (const char *path)
{
	if (fnmatch("*.avi", path, FNM_CASEFOLD) == 0 ||
	    fnmatch("*.mts", path, FNM_CASEFOLD) == 0) {
		return 0;
	}
	return -1;
}

static int metadata_file_is_image (const char *path)
{
	if (fnmatch("*.jpg", path, FNM_CASEFOLD) == 0 ||
	    fnmatch("*.jpeg", path, FNM_CASEFOLD) == 0) {
		return 0;
	}
	return -1;
}

static int metadata_audio (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_AUDIO;
	metadata->title = strdup(metadata->basename);
	if (fnmatch("*.mp3", path, FNM_CASEFOLD) == 0) {
		metadata->mimetype = strdup("audio/mpeg");
	}
	if (metadata->title == NULL ||
	    metadata->mimetype == NULL) {
		return -1;
	}
	return 0;
}

static int metadata_video (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_VIDEO;
	metadata->title = strdup(metadata->basename);
	if (fnmatch("*.avi", path, FNM_CASEFOLD) == 0) {
		metadata->mimetype = strdup("video/x-msvideo");
	} else if (fnmatch("*.mts", path, FNM_CASEFOLD) == 0) {
		metadata->mimetype = strdup("video/vnd.dlna.mpeg-tts");
	}
	if (metadata->title == NULL ||
	    metadata->mimetype == NULL) {
		return -1;
	}
	return 0;
}

static int metadata_image (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_IMAGE;
	metadata->title = strdup(metadata->basename);
	if (fnmatch("*.jpg", path, FNM_CASEFOLD) == 0 ||
	    fnmatch("*.jpeg", path, FNM_CASEFOLD) == 0) {
		metadata->mimetype = strdup("image/jpeg");
	}
	if (metadata->title == NULL ||
	    metadata->mimetype == NULL) {
		return -1;
	}
	return 0;
}

metadata_t * metadata_init (const char *path)
{
	char *p;
	struct stat stbuf;
	metadata_t *m;

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
		if (m->pathname == NULL || m->basename == NULL) {
			goto error;
		}

		if (metadata_file_is_video(path) == 0) {
			if (metadata_video(m) != 0) {
				goto error;
			}
		} else if (metadata_file_is_audio(path) == 0) {
			if (metadata_audio(m) != 0) {
				goto error;
			}
		} else if (metadata_file_is_image(path) == 0) {
			if (metadata_image(m) != 0) {
				goto error;
			}
		} else {
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
	free(metadata);
	return 0;
}
