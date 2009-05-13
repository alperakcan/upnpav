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

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
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

static int item_setup_uninit (sdl_item_t *item)
{
	free(item);
	return 0;
}

static sdl_item_t * item_setup_init (void)
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

typedef struct item_browser_s {
	sdl_item_t item;
	char *path;
} item_browser_t;

static int item_browser_uninit (sdl_item_t *item)
{
	item_browser_t *browser;
	browser = (item_browser_t *) item;
	free(browser->item.name);
	free(browser->path);
	free(browser);
	return 0;
}

static void item_browser_qsort (void *base, size_t nel, size_t width, int (*comp) (const void *, const void *))
{
	char tmp;
	size_t i;
	size_t j;
	size_t k;
	size_t wgap;
	if ((nel > 1) && (width > 0)) {
		assert(nel <= ((size_t) (-1)) / width); /* check for overflow */
		wgap = 0;
		do {
			wgap = 3 * wgap + 1;
		} while (wgap < (nel - 1) / 3);
		/* From the above, we know that either wgap == 1 < nel or */
		/* ((wgap-1)/3 < (int) ((nel-1)/3) <= (nel-1)/3 ==> wgap <  nel. */
		wgap *= width;	/* So this can not overflow if wnel doesn't. */
		nel *= width;	/* Convert nel to 'wnel' */
		do {
			i = wgap;
			do {
				j = i;
				do {
					register char *a;
					register char *b;
					j -= wgap;
					a = j + ((char *) base);
					b = a + wgap;
					if ((*comp)(a, b) <= 0) {
						break;
					}
					k = width;
					do {
						tmp = *a;
						*a++ = *b;
						*b++ = tmp;
					} while (--k);
				} while (j >= wgap);
				i += width;
			} while (i < nel);
			wgap = (wgap - width) / 3;
		} while (wgap);
	}
}

static int item_browser_scandir (const char *dir, struct dirent ***namelist, int (*selector) (const struct dirent *), int (*compar) (const void *, const void *))
{
	DIR *dp;
	int error;
	size_t pos;
	size_t names_size;
	struct dirent **names;
	struct dirent *current;
	dp = opendir(dir);
	if (dp == NULL) {
		return -1;
	}
	pos = 0;
	error = 0;
	names = NULL;
	names_size = 0;
	*namelist = NULL;
	while ((current = readdir(dp)) != NULL) {
		if (selector == NULL || (*selector)(current)) {
			struct dirent *vnew;
			size_t dsize;
			if (pos == names_size) {
				struct dirent **new;
				if (names_size == 0) {
					names_size = 10;
				} else {
					names_size *= 2;
				}
				new = (struct dirent **) realloc(names, names_size * sizeof (struct dirent *));
				if (new == NULL) {
					error = 1;
					break;
				}
				names = new;
			}
			dsize = &current->d_name[strlen(current->d_name) + 1] - (char *) current;
			vnew = (struct dirent *) malloc(dsize);
			if (vnew == NULL) {
				error = 2;
				break;
			}
			names[pos++] = (struct dirent *) memcpy(vnew, current, dsize);
		}
	}
	if (error != 0) {
		closedir(dp);
		while (pos > 0) {
			free(names[--pos]);
		}
		free(names);
		return -1;
	}
	closedir(dp);

	if (compar != NULL) {
		item_browser_qsort(names, pos, sizeof(struct dirent *), compar);
	}
	*namelist = names;
	return pos;
}

static int item_browser_filter (const struct dirent *entry)
{
	if (strcmp(entry->d_name, ".") == 0) {
		/* self */
		return 0;
	} else if (strcmp(entry->d_name, "..")  == 0) {
		/* parent */
		return 0;
	} else if (strncmp(entry->d_name, ".", 1) == 0) {
		/* hidden */
		return 0;
	}
	return 1;
}

static int item_browser_compare (const void *a, const void *b)
{
	return strcmp((*(const struct dirent **) a)->d_name, (*(const struct dirent **) b)->d_name);
}

static int item_browser_items (sdl_item_t *item, int *nitems, sdl_item_t ***items)
{
	int i;
	int n;
	struct stat stbuf;
	item_browser_t *itm;
	item_browser_t *browser;
	struct dirent **namelist;

	browser = (item_browser_t *) item;

	namelist = NULL;
	*items = NULL;
	*nitems = 0;

	n = item_browser_scandir(browser->path, &namelist, item_browser_filter, item_browser_compare);
	if (n < 0) {
		goto error;
	}

	*items = (sdl_item_t **) malloc(sizeof(sdl_item_t *) * n);
	if (*items == NULL) {
		goto error;
	}
	memset(*items, 0, sizeof(sdl_item_t *) * n);
	*nitems = 0;

	for (i = 0; i < n; i++) {
		itm = (item_browser_t *) malloc(sizeof(item_browser_t));
		if (itm == NULL) {
			continue;
		}
		memset(itm, 0, sizeof(item_browser_t));
		if (asprintf(&itm->path, "%s/%s", browser->path, namelist[i]->d_name) < 0) {
			free(itm);
			continue;
		}
		itm->item.name = strdup(namelist[i]->d_name);
		if (itm->item.name == NULL) {
			free(itm->path);
			free(itm);
			continue;
		}
		if (stat(itm->path, &stbuf) != 0) {
			free(itm->item.name);
			free(itm->path);
			free(itm);
			continue;
		}
		if (S_ISDIR(stbuf.st_mode)) {
			itm->item.type = SDL_ITEM_TYPE_CONTAINER;
			itm->item.items = item_browser_items;
			itm->item.uninit = item_browser_uninit;
			(*items)[*nitems] = &itm->item;
			*nitems = *nitems + 1;
		} else if (S_ISREG(stbuf.st_mode)) {
			itm->item.type = SDL_ITEM_TYPE_OBJECT;
			itm->item.uninit = item_browser_uninit;
			(*items)[*nitems] = &itm->item;
			*nitems = *nitems + 1;
		} else {
			free(itm->item.name);
			free(itm->path);
			free(itm);
		}
	}
	for (i = 0; i < n; i++) {
		free(namelist[i]);
	}
	free(namelist);
	return 0;
error:
	for (i = 0; i < *nitems; i++) {
		itm = (item_browser_t *) (*items)[i];
		free(itm->item.name);
		free(itm->path);
		free(itm);
	}
	free(*items);
	*items = NULL;
	*nitems = 0;
	for (i = 0; i < n; i++) {
		free(namelist[i]);
	}
	free(namelist);
	return -1;
}

static sdl_item_t * item_browser_init (void)
{
	item_browser_t *browser;
	browser = (item_browser_t *) malloc(sizeof(item_browser_t));
	if (browser == NULL) {
		return NULL;
	}
	browser->path = strdup("/");
	browser->item.name = strdup("Browser");
	browser->item.type = SDL_ITEM_TYPE_CONTAINER;
	browser->item.image = NULL;
	browser->item.info = NULL;
	browser->item.status = NULL;
	browser->item.items = item_browser_items;
	browser->item.uninit = item_browser_uninit;
	return &browser->item;
}

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
