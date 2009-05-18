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

typedef struct metadata_s metadata_t;
typedef struct metadata_audio_s metadata_audio_t;
typedef struct metadata_video_s metadata_video_t;
typedef struct metadata_image_s metadata_image_t;
typedef struct metadata_snapshot_s metadata_snapshot_t;

typedef enum {
	METADATA_TYPE_UNKNOWN,
	METADATA_TYPE_CONTAINER,
	METADATA_TYPE_AUDIO,
	METADATA_TYPE_VIDEO,
	METADATA_TYPE_IMAGE,
} metadata_type_t;

struct metadata_s {
	metadata_type_t type;
	char *pathname;
	char *basename;
	char *mimetype;
	char *dlnainfo;
	char *title;
	char *artist;
	char *album;
	char *genre;
	char *duration;
	int image_width;
	int image_height;
	unsigned char *image;
	size_t size;
};

metadata_t * metadata_init (const char *path);
int metadata_uninit (metadata_t *metadata);

metadata_snapshot_t * metadata_snapshot_init (const char *path, int width, int height);
int metadata_snapshot_uninit (metadata_snapshot_t *snapshot);
unsigned char * metadata_snapshot_obtain (metadata_snapshot_t *snapshot, unsigned int seconds);
