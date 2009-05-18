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

#include <math.h>
#include <SDL.h>
#include <cairo.h>

#include "controller.h"
#include "sdl.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct sdl_s {
	SDL_Surface *screen;
	int width;
	int height;
	int bitsperpixel;
	int pitch;

	cairo_t *cairo_cairo;
	SDL_Surface *sdl_surface;
	cairo_surface_t *cairo_surface;
	unsigned char *surface_data;

	int nprevs;
	sdl_item_t **prevs;
	int *pselected;

	int nitems;
	sdl_item_t **items;

	int selected;

	int button_yoffsey;
} sdl_t;

typedef struct cairogui_s {
	cairo_t *cairo;
	cairo_surface_t *surface;
} cairogui_t;

static sdl_t sdl;

static int cairogui_destroy (cairogui_t *cairogui)
{
	cairo_destroy(cairogui->cairo);
	cairo_surface_destroy(cairogui->surface);
	free(cairogui);
	return 0;
}

static cairogui_t * cairogui_create (int width, int height)
{
	cairogui_t *cairogui;
	cairogui = (cairogui_t *) malloc(sizeof(cairogui_t));
	if (cairogui == NULL) {
		return NULL;
	}
	memset(cairogui, 0, sizeof(cairogui_t));
	cairogui->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairogui->surface == NULL) {
		free(cairogui);
		return NULL;
	}
	cairogui->cairo = cairo_create(cairogui->surface);
	if (cairogui->cairo == NULL) {
		cairo_surface_destroy(cairogui->surface);
		free(cairogui);
		return NULL;
	}
	cairo_set_source_rgba(cairogui->cairo, 0.0, 0.0, 0.0, 0.0);
	cairo_fill(cairogui->cairo);
	cairo_paint(cairogui->cairo);
	return cairogui;
}

static cairogui_t * cairogui_create_background (int width, int height)
{
	return cairogui_create(width, height);
}

static cairogui_t * cairogui_create_button (int width, int height, int radius)
{
	double controlx;
	double controly;
	cairogui_t *cairogui;
	cairo_pattern_t *pattern;
	cairogui = cairogui_create(width, height);
	if (cairogui == NULL) {
		return NULL;
	}

	pattern = cairo_pattern_create_radial(radius, radius, 0, radius, radius, radius);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, 0, 0, radius, radius);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_radial(width - radius, radius, 0, width - radius, radius, radius);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, width - radius, 0, radius, radius);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_radial(width - radius, height - radius, 0, width - radius, height - radius, radius);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, width - radius, height - radius, radius, radius);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_radial(radius, height - radius, 0, radius, height - radius, radius);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, 0, height - radius, radius, radius);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_linear(radius, radius, 0, radius);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, 0, radius, radius, height - (radius * 2));
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_linear(width - radius, radius, width, radius);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, width - radius, radius, radius, height - (radius * 2));
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_linear(radius, radius, radius, 0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, radius, 0, width - (radius * 2), radius);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_linear(radius, height - radius, radius, height);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.6, 0.0, 0.0, 1.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 0.0, 0.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, radius, height - radius, width - (radius * 2), radius);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	pattern = cairo_pattern_create_linear(0, 0, 0, height * 0.6);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.2, 1.0, 1.0, 1.0, 0.3);
	cairo_pattern_add_color_stop_rgba(pattern, 1.0, 1.0, 1.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	controlx = 0.40 * radius;
	controly = 0.40 * radius;
	cairo_new_path(cairogui->cairo);
	cairo_move_to(cairogui->cairo, radius, controly);
	cairo_rel_line_to(cairogui->cairo, width - (radius * 2), 0);
	cairo_rel_curve_to(cairogui->cairo, controlx, 0, radius - controlx, controly, radius - controlx, radius - controly);
	cairo_rel_line_to(cairogui->cairo, 0, height * 0.6);
	cairo_rel_line_to(cairogui->cairo, -(width - (controlx * 2)), 0);
	cairo_rel_line_to(cairogui->cairo, 0, -(height * 0.6));
	cairo_rel_curve_to(cairogui->cairo, 0, -controly, controlx, -(radius - controly), radius - controlx, -(radius - controly));
	cairo_close_path(cairogui->cairo);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	return cairogui;
}

static cairogui_t * cairogui_create_list (int width, int height)
{
	int i;
	int c;
	int s;
	double y;
	sdl_item_t *item;
	cairogui_t *cairogui;
	cairo_pattern_t *pattern;
	cairo_font_extents_t extents;
	cairogui = cairogui_create(width, height);
	if (cairogui == NULL) {
		return NULL;
	}
	cairo_set_source_rgb(cairogui->cairo, 1.0, 1.0, 1.0);
	cairo_select_font_face(cairogui->cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cairogui->cairo, 22.00);
	cairo_font_extents(cairogui->cairo, &extents);

	if (sdl.nitems <= 11) {
		i = 0;
	} else {
		if (sdl.selected < 5) {
			i = 0;
		} else if (sdl.selected >= 5) {
			if (sdl.nitems - sdl.selected <= 5) {
				i = sdl.nitems - 11;
			} else {
				i = sdl.selected - 5;
			}
		}
	}
	s = i;

	y = extents.height + extents.ascent;
	for (c = 1; i < sdl.nitems; c++, i++) {
		item = sdl.items[i];

		pattern = cairo_pattern_create_linear(0, 0, width, 0);
		cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 1.0, 1.0, 1.0);
		cairo_pattern_add_color_stop_rgba(pattern, 0.7, 1.0, 1.0, 1.0, 1.0);
		cairo_pattern_add_color_stop_rgba(pattern, 0.9, 1.0, 1.0, 1.0, 0.0);
		cairo_set_source(cairogui->cairo, pattern);
		cairo_move_to(cairogui->cairo, 0, y - extents.ascent);
		cairo_show_text(cairogui->cairo, item->name);
		cairo_stroke(cairogui->cairo);
		cairo_pattern_destroy(pattern);

		if (item->type == SDL_ITEM_TYPE_CONTAINER && i == sdl.selected) {
			cairo_set_source_rgba(cairogui->cairo, 0.8, 0.8, 0.8, 1.0);
			cairo_set_line_width(cairogui->cairo, 6.00);
			cairo_set_line_cap(cairogui->cairo, CAIRO_LINE_CAP_ROUND);
			cairo_set_line_join(cairogui->cairo, CAIRO_LINE_JOIN_ROUND);
			cairo_move_to(cairogui->cairo, width - 20, y - 36.0);
			cairo_rel_line_to(cairogui->cairo, 8, 8);
			cairo_rel_line_to(cairogui->cairo, -8, 8);
			cairo_stroke(cairogui->cairo);
		}

		if (sdl.selected == i) {
			sdl.button_yoffsey = y - extents.ascent - 36;
		}
		y += 38.0;
		if (c >= 11) {
			break;
		}
	}

	pattern = cairo_pattern_create_linear(0, 0, 0, height);
	if (s > 0) {
		cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 0.0, 1.0);
	} else {
		cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 0.0, 0.0);
	}
	cairo_pattern_add_color_stop_rgba(pattern, 0.1, 0.0, 0.0, 0.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.9, 0.0, 0.0, 0.0, 0.0);
	if (i + 1 < sdl.nitems) {
		cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 0.0, 1.0);
	} else {
		cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 0.0, 0.0);
	}
	cairo_set_source(cairogui->cairo, pattern);
	cairo_rectangle(cairogui->cairo, 0, 0, width, height);
	cairo_fill(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	return cairogui;
}

static cairogui_t * cairogui_create_head (int width, int height)
{
	sdl_item_t *item;
	cairogui_t *cairogui;
	cairo_pattern_t *pattern;
	cairo_font_extents_t extents;
	cairogui = cairogui_create(width, height);
	if (cairogui == NULL) {
		return NULL;
	}
	if (sdl.nprevs <= 0) {
		return cairogui;
	}
	item = sdl.prevs[sdl.nprevs - 1];

	cairo_set_source_rgb(cairogui->cairo, 1.0, 1.0, 1.0);
	cairo_select_font_face(cairogui->cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cairogui->cairo, 22.00);
	cairo_font_extents(cairogui->cairo, &extents);

	pattern = cairo_pattern_create_linear(0, 0, width, 0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.9, 1.0, 1.0, 1.0, 0.0);
	cairo_set_source(cairogui->cairo, pattern);
	cairo_move_to(cairogui->cairo, 0.0, 30.0);
	cairo_show_text(cairogui->cairo, item->name);
	cairo_stroke(cairogui->cairo);
	cairo_pattern_destroy(pattern);

	cairo_set_source_rgba(cairogui->cairo, 0.5, 0.5, 0.5, 1.0);
	cairo_set_line_width(cairogui->cairo, 2.00);
	cairo_set_line_cap(cairogui->cairo, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cairogui->cairo, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to(cairogui->cairo, 1, height - 3);
	cairo_rel_line_to(cairogui->cairo, width - 2, 0);
	cairo_stroke(cairogui->cairo);

	return cairogui;
}

static cairogui_t * cairogui_create_preview (int width, int height)
{
	double scale;
	double scalex;
	double scaley;
	double xoffset;
	double yoffset;
	sdl_item_t *item;
	sdl_item_image_t image;
	cairo_matrix_t matrix;
	cairo_surface_t *surface;
	cairogui_t *cairogui;
	cairogui = cairogui_create(width, height);
	if (cairogui == NULL) {
		return NULL;
	}

	item = sdl.items[sdl.selected];
	if (item->image == NULL) {
		return cairogui;
	}
	if (item->image(item, &image) !=  0) {
		return cairogui;
	}
	if (image.type == SDL_IMAGE_TYPE_FILE) {
		surface = cairo_image_surface_create_from_png((char *) image.buffer);
		if (surface == NULL) {
			return cairogui;
		}
		image.width = cairo_image_surface_get_width(surface);
		image.height = cairo_image_surface_get_height(surface);
	} else if (image.type == SDL_IMAGE_TYPE_ARGB32) {
		surface = cairo_image_surface_create_for_data((unsigned char *) image.buffer, CAIRO_FORMAT_ARGB32, image.width, image.height, image.width * 4);
		image.width = cairo_image_surface_get_width(surface);
		image.height = cairo_image_surface_get_height(surface);
	} else {
		return cairogui;
	}

	scalex = (double) ((double) (width)) / ((double) image.width);
	scaley = (double) ((double) (height )) / ((double) image.height);
	scale = MIN(scalex, scaley);
	xoffset = (((double) (width )) - (((double) image.width) * scale)) / 2.0;
	yoffset = (((double) (height )) - (((double) image.height) * scale)) / 2.0;

	cairo_matrix_init(&matrix, scale, 0.0, 0.0, scale, xoffset, yoffset);
	cairo_transform(cairogui->cairo, &matrix);
	cairo_set_source_surface(cairogui->cairo, surface, 0, 0);
	cairo_rectangle(cairogui->cairo, 0, 0, image.width, image.height);
	cairo_paint(cairogui->cairo);
	cairo_surface_destroy(surface);

	return cairogui;
}

static int cairogui_update (void)
{
	cairogui_t *background;
	cairogui_t *head;
	cairogui_t *list;
	cairogui_t *button;
	cairogui_t *preview;

	background = cairogui_create_background(sdl.width, sdl.height);
	head = cairogui_create_head(370, 50);
	list = cairogui_create_list(370, 420);
	button = cairogui_create_button(400, 58, 20);
	preview = cairogui_create_preview(300, 420);

	cairo_set_source_rgba(sdl.cairo_cairo, 0.0, 0.0, 0.0, 1.0);
	cairo_fill(sdl.cairo_cairo);
	cairo_paint(sdl.cairo_cairo);

	cairo_set_source_surface(sdl.cairo_cairo, background->surface, 0, 0);
	cairo_paint(sdl.cairo_cairo);
	cairo_set_source_surface(sdl.cairo_cairo, list->surface, 340, 110);
	cairo_paint(sdl.cairo_cairo);
	cairo_set_source_surface(sdl.cairo_cairo, head->surface, 340, 50);
	cairo_paint(sdl.cairo_cairo);
	cairo_set_source_surface(sdl.cairo_cairo, preview->surface, 10, 110);
	cairo_paint(sdl.cairo_cairo);
	cairo_set_source_surface(sdl.cairo_cairo, button->surface, 320, 110 + sdl.button_yoffsey);
	cairo_paint(sdl.cairo_cairo);

	SDL_BlitSurface(sdl.sdl_surface, NULL, sdl.screen, NULL);
	SDL_Flip(sdl.screen);

	cairogui_destroy(button);
	cairogui_destroy(list);
	cairogui_destroy(preview);
	cairogui_destroy(head);
	cairogui_destroy(background);

	return 0;
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
	cairogui_update();
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
	free(sdl.pselected);
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
	sdl.pselected = NULL;
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
		sdl.pselected = (int *) realloc(sdl.pselected, sizeof(int) * (sdl.nprevs + 1));
		sdl.prevs[sdl.nprevs] = item;
		sdl.pselected[sdl.nprevs] = sdl.selected;
		sdl.nprevs++;

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
	int selected;
	sdl_item_t *item;
	sdl_item_t **items;

	if (sdl.nprevs <= 1) {
		sdl_gui_uninit();
		sdl_gui_init();
		return 0;
	}

	item = sdl.prevs[sdl.nprevs - 2];
	selected = sdl.pselected[sdl.nprevs - 1];

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
	sdl.selected = selected;

	item = sdl.prevs[sdl.nprevs - 1];
	if (item->uninit != NULL) {
		item->uninit(item);
	}
	sdl.prevs = (sdl_item_t **) realloc(sdl.prevs, sizeof(sdl_item_t *) * (sdl.nprevs - 1));
	sdl.pselected = (int *) realloc(sdl.pselected, sizeof(sdl_item_t *) * (sdl.nprevs - 1));
	sdl.nprevs--;

	sdl_gui_update();
	return 0;
}

static int sdl_loop (void)
{
	SDL_Event event;
	memset(&event, 0, sizeof(SDL_Event));
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
		memset(&event, 0, sizeof(SDL_Event));
	}
	return 0;
}

static int sdl_main (char *options)
{
	memset(&sdl, 0, sizeof(sdl_t));

	sdl.width = 720;
	sdl.height = 576;
	sdl.bitsperpixel = 32;
	sdl.pitch = sdl.width * ((sdl.bitsperpixel + 7) / 8);

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

	sdl.surface_data = (unsigned char *) malloc(sdl.height * sdl.pitch);
	memset(sdl.surface_data, 0x00, sdl.height * sdl.pitch);
	sdl.sdl_surface = SDL_CreateRGBSurfaceFrom(sdl.surface_data, sdl.width, sdl.height, sdl.bitsperpixel, sdl.pitch, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	sdl.cairo_surface = cairo_image_surface_create_for_data(sdl.surface_data, CAIRO_FORMAT_ARGB32, sdl.width, sdl.height, sdl.pitch);
	sdl.cairo_cairo = cairo_create(sdl.cairo_surface);
	cairo_set_operator(sdl.cairo_cairo, CAIRO_OPERATOR_OVER);

	sdl_gui_init();

	sdl_loop();

	sdl_gui_uninit();

	cairo_surface_destroy(sdl.cairo_surface);
	cairo_destroy(sdl.cairo_cairo);
	SDL_FreeSurface(sdl.sdl_surface);
	free(sdl.surface_data);

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
