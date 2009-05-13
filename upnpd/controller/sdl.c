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

#include <SDL.h>

#include "controller.h"
#include "sdl.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct sdl_s {
	SDL_Surface *screen;
	int width;
	int height;
	int bitsperpixel;
	sdl_item_t **prevs;
	int nprevs;
	sdl_item_t **items;
	int nitems;
	int selected;
	int pselected;
} sdl_t;

static sdl_t sdl;

static int sdl_gui_update (void)
{
	int i;
	sdl_item_t *item;
	printf("\33[2J");
	if (sdl.nprevs > 0) {
		printf("browsing '%s'\n", sdl.prevs[sdl.nprevs - 1]->name);
	}
	for (i = 0; i < sdl.nitems; i++) {
		item = sdl.items[i];
		printf("  %s '%s' (%s)\n",
			(i == sdl.selected) ? "*" : " ",
			item->name,
			(item->type == SDL_ITEM_TYPE_CONTAINER) ? "container" :
			(item->type == SDL_ITEM_TYPE_OBJECT) ? "object" :
			(item->type == SDL_ITEM_TYPE_INFO) ? "info" :
			"unknown");
	}
	return 0;
}

static int sdl_gui_uninit (void)
{
	int i;
	for (i = 0; i < sdl.nprevs; i++) {
		if (sdl.prevs[i]->uninit != NULL) {
			sdl.prevs[i]->uninit(sdl.prevs[i]);
		}
	}
	free(sdl.prevs);
	for (i = 0; i < sdl.nitems; i++) {
		if (sdl.items[i]->uninit != NULL) {
			sdl.items[i]->uninit(sdl.items[i]);
		}
	}
	free(sdl.items);
	return 0;
}

static int sdl_gui_init (void)
{
	sdl.prevs = NULL;
	sdl.nprevs = 0;
	sdl.items = (sdl_item_t **) malloc(sizeof(sdl_item_t *) * 2);
	if (sdl.items == NULL) {
		return -1;
	}
	sdl.items[0] = item_browser_init();
	sdl.items[1] = item_setup_init();
	sdl.nitems = 2;
	sdl.selected = 0;
	sdl.pselected = 0;
	sdl_gui_update();
	return 0;
}

static int sdl_keyup (void)
{
	sdl.selected = MAX(sdl.selected - 1, 0);
	sdl_gui_update();
	return 0;
}

static int sdl_keydown (void)
{
	sdl.selected = MIN(sdl.selected + 1, sdl.nitems - 1);
	sdl_gui_update();
	return 0;
}

static int sdl_keyspace (void)
{
	int i;
	int nitems;
	sdl_item_t *item;
	sdl_item_t **items;
	item = sdl.items[sdl.selected];
	if (item->type == SDL_ITEM_TYPE_CONTAINER) {
		if (item->items == NULL) {
			return -1;
		}

		if (item->items(item, &nitems, &items) != 0) {
			return -1;
		}
		if (nitems <= 0 || items == NULL) {
			return -1;
		}

		for (i = 0; i < sdl.nitems; i++) {
			if (sdl.items[i] == item) {
				continue;
			}
			if (sdl.items[i]->uninit != NULL) {
				sdl.items[i]->uninit(sdl.items[i]);
			}
		}

		sdl.prevs = (sdl_item_t **) realloc(sdl.prevs, sizeof(sdl_item_t *) * (sdl.nprevs + 1));
		sdl.prevs[sdl.nprevs] = item;
		sdl.nprevs++;
		sdl.pselected = sdl.selected;

		free(sdl.items);
		sdl.items = items;
		sdl.nitems = nitems;
		sdl.selected = 0;

		sdl_gui_update();
	}
	return 0;
}

static int sdl_keyescape (void)
{
	int i;
	int nitems;
	sdl_item_t *item;
	sdl_item_t **items;

	if (sdl.nprevs <= 1) {
		sdl_gui_uninit();
		sdl_gui_init();
		return 0;
	}

	item = sdl.prevs[sdl.nprevs - 2];
	if (item->items(item, &nitems, &items) != 0) {
		return -1;
	}
	if (nitems <= 0 || items == NULL) {
		return -1;
	}

	for (i = 0; i < sdl.nitems; i++) {
		if (sdl.items[i]->uninit != NULL) {
			sdl.items[i]->uninit(sdl.items[i]);
		}
	}
	free(sdl.items);
	sdl.items = items;
	sdl.nitems = nitems;
	sdl.selected = sdl.pselected;

	item = sdl.prevs[sdl.nprevs - 1];
	if (item->uninit != NULL) {
		item->uninit(item);
	}
	sdl.prevs = (sdl_item_t **) realloc(sdl.prevs, sizeof(sdl_item_t *) * (sdl.nprevs - 1));
	sdl.nprevs--;

	sdl_gui_update();
	return 0;
}

static int sdl_loop (void)
{
	SDL_Event event;
	while (SDL_WaitEvent(&event) >= 0) {
		switch (event.type) {
			case SDL_ACTIVEEVENT:
				break;
			case SDL_KEYUP:
				if (event.key.keysym.sym == SDLK_q) {
					return 0;
				} else if (event.key.keysym.sym == SDLK_UP) {
					sdl_keyup();
				} else if (event.key.keysym.sym == SDLK_DOWN) {
					sdl_keydown();
				} else if (event.key.keysym.sym == SDLK_SPACE) {
					sdl_keyspace();
				} else if (event.key.keysym.sym == SDLK_ESCAPE) {
					sdl_keyescape();
				}
				break;
			case SDL_KEYDOWN:
				break;
			case SDL_QUIT:
				return 0;
			default:
				break;

		}
	}
	return 0;
}

static int sdl_main (char *options)
{
	memset(&sdl, 0, sizeof(sdl_t));

	sdl.width = 720;
	sdl.height = 576;
	sdl.bitsperpixel = 32;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		printf("SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
		return -1;
	}

	sdl.screen = SDL_SetVideoMode(sdl.width, sdl.height, sdl.bitsperpixel, SDL_HWSURFACE | SDL_SWSURFACE);
	if (sdl.screen == NULL) {
		printf("SDL_SetVideoMode(720, 576, 32, SDL_HWSURFACE) failed: %s\n", SDL_GetError());
		if(SDL_WasInit(SDL_INIT_VIDEO)) {
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
		}
		if(SDL_WasInit(SDL_INIT_TIMER)) {
			SDL_QuitSubSystem(SDL_INIT_TIMER);
		}
		return -1;
	}
	SDL_WM_SetCaption("Basic Multimedia Player", NULL);

	sdl_gui_init();

	sdl_loop();

	sdl_gui_uninit();

	if(SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
	if(SDL_WasInit(SDL_INIT_TIMER)) {
		SDL_QuitSubSystem(SDL_INIT_TIMER);
	}
	SDL_Quit();

	return 0;
}

controller_gui_t controller_sdl = {
	.name = "sdl",
	.main = sdl_main,
};
