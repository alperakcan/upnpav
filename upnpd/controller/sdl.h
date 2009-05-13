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

typedef struct sdl_rect_s sdl_rect_t;
typedef struct sdl_item_s sdl_item_t;
typedef struct sdl_item_info_s sdl_item_info_t;
typedef struct sdl_item_image_s sdl_item_image_t;
typedef struct sdl_item_status_s sdl_item_status_t;

struct sdl_rect_s {
	int x;
	int y;
	int w;
	int h;
};

typedef enum {
	SDL_ITEM_TYPE_UNKNOWN,
	SDL_ITEM_TYPE_CONTAINER,
	SDL_ITEM_TYPE_OBJECT,
	SDL_ITEM_TYPE_INFO,
} sdl_item_type_t;

typedef enum {
	SDL_IMAGE_TYPE_UNKNWON,
	SDL_IMAGE_TYPE_FILE,
	SDL_IMAGE_TYPE_RGB24,
} sdl_item_image_type_t;

struct sdl_item_image_t {
	sdl_item_image_type_t type;
	unsigned char *buffer;
	unsigned int width;
	unsigned int height;
};

struct sdl_item_info_s {
	char *artist;
	char *album;
	char *description;
	unsigned int duration;
};

struct sdl_item_status_s {
	char *text;
};

struct sdl_item_s {
	char *name;
	sdl_item_type_t type;
	int (*image) (sdl_item_t *item, sdl_item_image_t *image);
	int (*info) (sdl_item_t *item, sdl_item_info_t *info);
	int (*status) (sdl_item_t *item, sdl_item_status_t *status);
	int (*items) (sdl_item_t *item, int *nitems, sdl_item_t ***items);
	int (*uninit) (sdl_item_t *item);
};
