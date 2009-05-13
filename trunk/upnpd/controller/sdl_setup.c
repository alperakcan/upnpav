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
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sdl.h"

static int item_setup_uninit (sdl_item_t *item)
{
	free(item);
	return 0;
}

sdl_item_t * item_setup_init (void)
{
	sdl_item_t *item;
	item = (sdl_item_t *) malloc(sizeof(sdl_item_t));
	if (item == NULL) {
		return NULL;
	}
	memset(item, 0, sizeof(sdl_item_t));
	item->name = "Settings";
	item->type = SDL_ITEM_TYPE_CONTAINER;
	item->image = NULL;
	item->info = NULL;
	item->status = NULL;
	item->items = NULL;
	item->uninit = item_setup_uninit;
	return item;
}
