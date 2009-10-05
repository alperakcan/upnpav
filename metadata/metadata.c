/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 CoreCodec, Inc., http://www.CoreCodec.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Any non-LGPL usage of this software or parts of this software is strictly
 * forbidden.
 *
 * Commercial non-LGPL licensing of this software is possible.
 * For more info contact CoreCodec through info@corecodec.com
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

#include "platform.h"
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
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.ts", "video/mpeg", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mpg", "video/mpeg", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mpeg", "video/mpeg", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.avi", "video/x-msvideo", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mp4", "video/mp4", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.wmv", "video/x-ms-wmv", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mkv", "video/x-matroska", metadata_video},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.mts", "video/vnd.dlna.mpeg-tts", metadata_video},
	/* audio */
	& (metadata_info_t) {METADATA_TYPE_AUDIO, "*.mp3", "audio/mpeg", metadata_audio},
	& (metadata_info_t) {METADATA_TYPE_AUDIO, "*.ogg", "audio/ogg", metadata_audio},
	& (metadata_info_t) {METADATA_TYPE_VIDEO, "*.wma", "audio/x-ms-wma", metadata_audio},
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
	time_t mtime;
	metadata_t *m;
	file_stat_t stat;
	struct tm modtime;
	metadata_info_t **info;

	if (file_access(path, FILE_MODE_READ) != 0) {
		return NULL;
	}
	if (file_stat(path, &stat) != 0) {
		return NULL;
	}

	m = NULL;
	if (FILE_ISREG(stat.type)) {
		m = (metadata_t *) malloc(sizeof(metadata_t));
		if (m == NULL) {
			return NULL;
		}
		memset(m, 0, sizeof(metadata_t));

		m->type = METADATA_TYPE_UNKNOWN;
		p = strrchr(path, '/');
		m->pathname = strdup(path);
		m->basename = (p == NULL) ? strdup(path) : strdup(p + 1);
		m->size = stat.size;
		m->date = (char *) malloc(sizeof(char) * 30);
		if (m->date != NULL) {
			mtime = stat.mtime;
			localtime_r(&mtime, &modtime);
			stat.mtime = mtime;
			strftime(m->date, 30, "%F %T", &modtime);
		}
		for (info = metadata_info; *info != NULL; info++) {
			if (file_match((*info)->extension, m->basename, FILE_MATCH_CASEFOLD) == 0) {
				m->type = (*info)->type;
				m->mimetype = strdup((*info)->mimetype);
				break;
			}
		}
		if (m->type == METADATA_TYPE_UNKNOWN ||
		    m->pathname == NULL ||
		    m->basename == NULL ||
		    m->mimetype == NULL ||
		    m->date == NULL) {
			goto error;
		}
		if ((*info)->metadata(m) != 0) {
			goto error;
		}
		return m;
	} else if (FILE_ISDIR(stat.type)) {
		m = (metadata_t *) malloc(sizeof(metadata_t));
		if (m == NULL) {
			return NULL;
		}
		memset(m, 0, sizeof(metadata_t));

		m->type = METADATA_TYPE_CONTAINER;
		p = strrchr(path, '/');
		m->pathname = strdup(path);
		m->basename = (p == NULL) ? strdup(path) : strdup(p + 1);
		m->size = stat.size;
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
	free(metadata->date);
	free(metadata->title);
	free(metadata->artist);
	free(metadata->album);
	free(metadata->genre);
	free(metadata->duration);
	free(metadata->image);
	free(metadata);
	return 0;
}
