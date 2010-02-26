/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 - 2010 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 - 2010 CoreCodec, Inc., http://www.CoreCodec.com
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

#if defined(HAVE_LIBFFMPEG)
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#endif

#include "platform.h"
#include "metadata.h"

typedef struct metadata_info_s {
	metadata_type_t type;
	char *extension;
	char *mimetype;
	int (*metadata) (metadata_t *metadata);
} metadata_info_t;

static int metadata_video_ffmpeg (metadata_t *metadata)
{
#if defined(HAVE_LIBFFMPEG)
	int i;
	int astream;
	int vstream;
	AVFormatContext *ctx;
	av_register_all();
	if (av_open_input_file(&ctx, metadata->pathname, NULL, 0, NULL) != 0) {
		debugf("av_open_input_file(%s) failed", metadata->pathname);
		return 0;
	}
	av_find_stream_info(ctx);
	astream = -1;
	vstream = -1;
	for (i = 0; i < ctx->nb_streams; i++) {
		if (astream == -1 && ctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
			astream = i;
		}
		if (vstream == -1 && ctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			vstream = i;
		}
	}
	if (vstream == -1) {
		debugf("%s is not a video file", metadata->pathname);
		av_close_input_file(ctx);
		return -1;
	}
	debugf("%s info:", metadata->basename);
	debugf("  resolution: %d, %d", ctx->streams[vstream]->codec->width, ctx->streams[vstream]->codec->height);
	if (ctx->bit_rate > 8) {
		debugf("  bitrate: %u", ctx->bit_rate / 8);
	}
	if (ctx->duration > 0) {
		int duration = (int) (ctx->duration / AV_TIME_BASE);
		int hours = (int) (duration / 3600);
		int min = (int) (duration / 60 % 60);
		int sec = (int) (duration % 60);
		int ms = (int) (ctx->duration / (AV_TIME_BASE / 1000) % 1000);
		debugf("  duration: %d:%02d:%02d.%03d", hours, min, sec, ms);
		asprintf(&metadata->duration, "%d:%02d:%02d.%03d", hours, min, sec, ms);
	}
	switch (ctx->streams[vstream]->codec->codec_id) {
		default:
			debugf("unknown codec_id: 0x%08x (%u)", ctx->streams[vstream]->codec->codec_id, ctx->streams[vstream]->codec->codec_id);
			break;
	}
	av_close_input_file(ctx);
#endif
	return 0;
}

static int metadata_video (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_VIDEO;
	metadata->title = strdup(metadata->basename);
	if (metadata->title == NULL) {
		return -1;
	}
	metadata_video_ffmpeg(metadata);
	metadata->dlnainfo = strdup("*");
	return 0;
}

static int metadata_audio (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_AUDIO;
	metadata->title = strdup(metadata->basename);
	if (metadata->title == NULL) {
		return -1;
	}
	metadata->dlnainfo = strdup("*");
	return 0;
}

static int metadata_image (metadata_t *metadata)
{
	metadata->type = METADATA_TYPE_IMAGE;
	metadata->title = strdup(metadata->basename);
	if (metadata->title == NULL) {
		return -1;
	}
	metadata->dlnainfo = strdup("*");
	return 0;
}

static metadata_info_t metadata_info[] = {
	/* video */
	{ METADATA_TYPE_VIDEO, "*.ts", "video/mpeg", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.mpg", "video/mpeg", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.mpeg", "video/mpeg", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.avi", "video/x-msvideo", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.mp4", "video/mp4", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.wmv", "video/x-ms-wmv", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.mkv", "video/x-matroska", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.mts", "video/vnd.dlna.mpeg-tts", metadata_video },
	{ METADATA_TYPE_VIDEO, "*.flv", "video/x-flv", metadata_video },
	/* audio */
	{ METADATA_TYPE_AUDIO, "*.mp3", "audio/mpeg", metadata_audio },
	{ METADATA_TYPE_AUDIO, "*.ogg", "audio/ogg", metadata_audio },
	{ METADATA_TYPE_VIDEO, "*.wma", "audio/x-ms-wma", metadata_audio },
	/* image */
	{ METADATA_TYPE_IMAGE, "*.bmp", "image/bmp", metadata_image },
	{ METADATA_TYPE_IMAGE, "*.jpg", "image/jpeg", metadata_image },
	{ METADATA_TYPE_IMAGE, "*.jpeg", "image/jpeg", metadata_image },
	{ METADATA_TYPE_IMAGE, "*.png", "image/png", metadata_image },
	{ METADATA_TYPE_IMAGE, "*.tiff", "image/tiff", metadata_image },
	{ METADATA_TYPE_UNKNOWN },
};

metadata_t * metadata_init (const char *path)
{
	int i;
	char *p;
	time_t mtime;
	metadata_t *m;
	file_stat_t stat;
	struct tm modtime;
	metadata_info_t *info;

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
		for (i = 0; (info = &metadata_info[i])->type != METADATA_TYPE_UNKNOWN; i++) {
			if (file_match(info->extension, m->basename, FILE_MATCH_CASEFOLD) == 0) {
				m->type = info->type;
				m->mimetype = strdup(info->mimetype);
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
		if (info->metadata(m) != 0) {
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
	free(metadata);
	return 0;
}
