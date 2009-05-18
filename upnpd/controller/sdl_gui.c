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
#include "sdl_gui.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct cairogui_s {
	cairo_t *cairo;
	cairo_surface_t *surface;
	int width;
	int height;
	int xoffset;
	int yoffset;
	void *data;
} cairogui_t;

typedef struct cairogui_list_s {
	cairogui_t *cairogui;
	cairo_font_extents_t extents;
} cairogui_list_t;

typedef struct cairogui_head_s {
	cairogui_t *cairogui;
	cairo_font_extents_t extents;
	int nprevs;
} cairogui_head_t;

typedef struct sdlgui_s {
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

	cairogui_t *buttongui;
	cairogui_t *headgui;
	cairogui_t *listgui;
	cairogui_t *previewgui;
} sdlgui_t;

static sdlgui_t sdlgui;

static int cairogui_destroy (cairogui_t *cairogui)
{
	cairo_destroy(cairogui->cairo);
	cairo_surface_destroy(cairogui->surface);
	free(cairogui);
	return 0;
}

static cairogui_t * cairogui_create (int width, int height, int xoffset, int yoffset)
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
	cairogui->width = width;
	cairogui->height = height;
	cairogui->xoffset = xoffset;
	cairogui->yoffset = yoffset;
	return cairogui;
}

static cairogui_t * cairogui_create_button (int width, int height, int xoffset, int yoffset, int radius)
{
	double controlx;
	double controly;
	cairogui_t *cairogui;
	cairo_pattern_t *pattern;
	cairogui = cairogui_create(width, height, xoffset, yoffset);
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

static int cairogui_draw_list (cairogui_t *listgui)
{
	int i;
	int c;
	int s;
	double y;
	sdl_item_t *item;
	cairo_pattern_t *pattern;
	cairogui_list_t *cairogui_list;

	cairogui_list = listgui->data;

	cairo_set_source_rgba(listgui->cairo, 0.0, 0.0, 0.0, 1.0);
	cairo_fill(listgui->cairo);
	cairo_paint(listgui->cairo);

	if (sdlgui.nitems <= 11) {
		i = 0;
	} else {
		if (sdlgui.selected < 5) {
			i = 0;
		} else if (sdlgui.selected >= 5) {
			if (sdlgui.nitems - sdlgui.selected <= 5) {
				i = sdlgui.nitems - 11;
			} else {
				i = sdlgui.selected - 5;
			}
		}
	}
	s = i;

	pattern = cairo_pattern_create_linear(0, 0, listgui->width, 0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.9, 1.0, 1.0, 1.0, 0.0);
	cairo_set_source(listgui->cairo, pattern);

	y = cairogui_list->extents.height + cairogui_list->extents.ascent;
	for (c = 1; i < sdlgui.nitems; c++, i++) {
		item = sdlgui.items[i];

		cairo_move_to(listgui->cairo, 0, y - cairogui_list->extents.ascent);
		cairo_show_text(listgui->cairo, item->name);
		cairo_stroke(listgui->cairo);

		if (sdlgui.selected == i) {
			sdlgui.buttongui->yoffset = y - cairogui_list->extents.ascent - 36.0;
		}
		y += 38.0;
		if (c >= 11) {
			break;
		}
	}
	cairo_pattern_destroy(pattern);

	item = sdlgui.items[sdlgui.selected];
	cairo_set_source_rgba(listgui->cairo, 0.8, 0.8, 0.8, 1.0);
	cairo_set_line_width(listgui->cairo, 6.00);
	cairo_set_line_cap(listgui->cairo, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(listgui->cairo, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to(listgui->cairo, listgui->width - 20, sdlgui.buttongui->yoffset + cairogui_list->extents.ascent);
	cairo_rel_line_to(listgui->cairo, 8, 8);
	cairo_rel_line_to(listgui->cairo, -8, 8);
	cairo_stroke(listgui->cairo);

	pattern = cairo_pattern_create_linear(0, 0, 0, listgui->height);
	if (s > 0) {
		cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 0.0, 1.0);
	} else {
		cairo_pattern_add_color_stop_rgba(pattern, 0.0, 0.0, 0.0, 0.0, 0.0);
	}
	cairo_pattern_add_color_stop_rgba(pattern, 0.1, 0.0, 0.0, 0.0, 0.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.9, 0.0, 0.0, 0.0, 0.0);
	if (i + 1 < sdlgui.nitems) {
		cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 0.0, 1.0);
	} else {
		cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 0.0, 0.0);
	}
	cairo_set_source(listgui->cairo, pattern);
	cairo_rectangle(listgui->cairo, 0, 0, listgui->width, listgui->height);
	cairo_fill(listgui->cairo);
	cairo_pattern_destroy(pattern);

	return 0;
}

static cairogui_t * cairogui_create_list (int width, int height, int xoffset, int yoffset)
{
	cairogui_list_t *cairogui_list;
	cairogui_list = (cairogui_list_t *) malloc(sizeof(cairogui_list_t));
	if (cairogui_list == NULL) {
		return NULL;
	}
	memset(cairogui_list, 0, sizeof(cairogui_list_t));
	cairogui_list->cairogui = cairogui_create(width, height, xoffset, yoffset);
	if (cairogui_list->cairogui == NULL) {
		free(cairogui_list);
		return NULL;
	}
	cairo_set_source_rgb(cairogui_list->cairogui->cairo, 1.0, 1.0, 1.0);
	cairo_select_font_face(cairogui_list->cairogui->cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cairogui_list->cairogui->cairo, 22.00);
	cairo_font_extents(cairogui_list->cairogui->cairo, &cairogui_list->extents);

	cairogui_list->cairogui->data = cairogui_list;
	return cairogui_list->cairogui;
}

static int cairogui_draw_head (cairogui_t *headgui)
{
	sdl_item_t *item;
	cairo_pattern_t *pattern;
	cairogui_head_t *cairogui_head;

	cairogui_head = headgui->data;

	cairo_set_source_rgba(headgui->cairo, 0.0, 0.0, 0.0, 1.0);
	cairo_fill(headgui->cairo);
	cairo_paint(headgui->cairo);

	if (sdlgui.nprevs <= 0 || cairogui_head->nprevs == sdlgui.nprevs) {
		return 0;
	}
	item = sdlgui.prevs[sdlgui.nprevs - 1];

	pattern = cairo_pattern_create_linear(0, 0, headgui->width, 0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.7, 1.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgba(pattern, 0.9, 1.0, 1.0, 1.0, 0.0);
	cairo_set_source(headgui->cairo, pattern);
	cairo_move_to(headgui->cairo, 0.0, 30.0);
	cairo_show_text(headgui->cairo, item->name);
	cairo_stroke(headgui->cairo);
	cairo_pattern_destroy(pattern);

	cairo_set_source_rgba(headgui->cairo, 0.5, 0.5, 0.5, 1.0);
	cairo_set_line_width(headgui->cairo, 2.00);
	cairo_set_line_cap(headgui->cairo, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(headgui->cairo, CAIRO_LINE_JOIN_ROUND);
	cairo_move_to(headgui->cairo, 1, headgui->height - 3);
	cairo_rel_line_to(headgui->cairo, headgui->width - 2, 0);
	cairo_stroke(headgui->cairo);

	return 0;
}

static cairogui_t * cairogui_create_head (int width, int height, int xoffset, int yoffset)
{
	cairogui_head_t *cairogui_head;
	cairogui_head = (cairogui_head_t *) malloc(sizeof(cairogui_head_t));
	if (cairogui_head == NULL) {
		return NULL;
	}
	memset(cairogui_head, 0, sizeof(cairogui_head_t));
	cairogui_head->cairogui = cairogui_create(width, height, xoffset, yoffset);
	if (cairogui_head->cairogui == NULL) {
		free(cairogui_head);
		return NULL;
	}
	cairo_set_source_rgb(cairogui_head->cairogui->cairo, 1.0, 1.0, 1.0);
	cairo_select_font_face(cairogui_head->cairogui->cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cairogui_head->cairogui->cairo, 22.00);
	cairo_font_extents(cairogui_head->cairogui->cairo, &cairogui_head->extents);

	cairogui_head->cairogui->data = cairogui_head;
	return cairogui_head->cairogui;
}

static int cairogui_draw_preview (cairogui_t *previewgui)
{
	double scale;
	double scalex;
	double scaley;
	double xoffset_;
	double yoffset_;
	sdl_item_t *item;
	sdl_item_image_t image;
	cairo_matrix_t matrix;
	cairo_surface_t *surface;

	cairo_save(previewgui->cairo);
	cairo_set_source_rgba(previewgui->cairo, 0.0, 0.0, 0.0, 1.0);
	cairo_rectangle(previewgui->cairo, 0, 0, previewgui->width, previewgui->height);
	cairo_paint(previewgui->cairo);

	item = sdlgui.items[sdlgui.selected];
	if (item->image == NULL) {
		return 0;
	}
	if (item->image(item, &image) !=  0) {
		return 0;
	}
	if (image.type == SDL_IMAGE_TYPE_FILE) {
		surface = cairo_image_surface_create_from_png((char *) image.buffer);
		if (surface == NULL) {
			free(image.buffer);
			return 0;
		}
		image.width = cairo_image_surface_get_width(surface);
		image.height = cairo_image_surface_get_height(surface);
	} else if (image.type == SDL_IMAGE_TYPE_ARGB32) {
		surface = cairo_image_surface_create_for_data((unsigned char *) image.buffer, CAIRO_FORMAT_ARGB32, image.width, image.height, image.width * 4);
		image.width = cairo_image_surface_get_width(surface);
		image.height = cairo_image_surface_get_height(surface);
	} else {
		free(image.buffer);
		return 0;
	}

	scalex = (double) ((double) (previewgui->width)) / ((double) image.width);
	scaley = (double) ((double) (previewgui->height )) / ((double) image.height);
	scale = MIN(scalex, scaley);
	xoffset_ = (((double) (previewgui->width )) - (((double) image.width) * scale)) / 2.0;
	yoffset_ = (((double) (previewgui->height )) - (((double) image.height) * scale)) / 2.0;

	cairo_matrix_init(&matrix, scale, 0.0, 0.0, scale, xoffset_, yoffset_);
	cairo_transform(previewgui->cairo, &matrix);
	cairo_set_source_surface(previewgui->cairo, surface, 0, 0);
	cairo_rectangle(previewgui->cairo, 0, 0, image.width, image.height);
	cairo_paint(previewgui->cairo);
	cairo_surface_destroy(surface);

	cairo_restore(previewgui->cairo);

	free(image.buffer);
	return 0;
}

static cairogui_t * cairogui_create_preview (int width, int height, int xoffset, int yoffset)
{
	cairogui_t *cairogui;
	cairogui = cairogui_create(width, height, xoffset, yoffset);
	if (cairogui == NULL) {
		return NULL;
	}
	return cairogui;
}

static int cairogui_update (void)
{
	cairogui_draw_list(sdlgui.listgui);
	cairogui_draw_head(sdlgui.headgui);
	cairogui_draw_preview(sdlgui.previewgui);

	cairo_set_source_rgba(sdlgui.cairo_cairo, 0.0, 0.0, 0.0, 1.0);
	cairo_fill(sdlgui.cairo_cairo);
	cairo_paint(sdlgui.cairo_cairo);

	cairo_set_source_surface(sdlgui.cairo_cairo, sdlgui.listgui->surface, sdlgui.listgui->xoffset, sdlgui.listgui->yoffset);
	cairo_paint(sdlgui.cairo_cairo);
	cairo_set_source_surface(sdlgui.cairo_cairo, sdlgui.headgui->surface, sdlgui.headgui->xoffset, sdlgui.headgui->yoffset);
	cairo_paint(sdlgui.cairo_cairo);
	cairo_set_source_surface(sdlgui.cairo_cairo, sdlgui.previewgui->surface, sdlgui.previewgui->xoffset, sdlgui.previewgui->yoffset);
	cairo_paint(sdlgui.cairo_cairo);
	cairo_set_source_surface(sdlgui.cairo_cairo, sdlgui.buttongui->surface, sdlgui.buttongui->xoffset, sdlgui.listgui->yoffset + sdlgui.buttongui->yoffset);
	cairo_paint(sdlgui.cairo_cairo);

	SDL_BlitSurface(sdlgui.sdl_surface, NULL, sdlgui.screen, NULL);
	SDL_Flip(sdlgui.screen);

	return 0;
}

static int sdl_gui_update (void)
{
	int i;
	sdl_item_t *item;
	printf("\33[2J");
	if (sdlgui.nprevs > 0) {
		printf("browsing '%s'\n", sdlgui.prevs[sdlgui.nprevs - 1]->name);
	}
	for (i = 0; i < sdlgui.nitems; i++) {
		item = sdlgui.items[i];
		printf("  %s '%s' (%s)\n",
			(i == sdlgui.selected) ? "*" : " ",
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
	for (i = 0; i < sdlgui.nprevs; i++) {
		if (sdlgui.prevs[i]->uninit != NULL) {
			sdlgui.prevs[i]->uninit(sdlgui.prevs[i]);
		}
	}
	free(sdlgui.prevs);
	free(sdlgui.pselected);
	for (i = 0; i < sdlgui.nitems; i++) {
		if (sdlgui.items[i]->uninit != NULL) {
			sdlgui.items[i]->uninit(sdlgui.items[i]);
		}
	}
	free(sdlgui.items);
	return 0;
}

static int sdl_gui_init (void)
{
	sdlgui.prevs = NULL;
	sdlgui.pselected = NULL;
	sdlgui.nprevs = 0;
	sdlgui.items = (sdl_item_t **) malloc(sizeof(sdl_item_t *) * 2);
	if (sdlgui.items == NULL) {
		return -1;
	}
	sdlgui.items[0] = item_browser_init();
	sdlgui.items[1] = item_setup_init();
	sdlgui.nitems = 2;
	sdlgui.selected = 0;
	sdlgui.pselected = 0;
	sdl_gui_update();
	return 0;
}

static int sdl_keyup (void)
{
	sdlgui.selected = MAX(sdlgui.selected - 1, 0);
	sdl_gui_update();
	return 0;
}

static int sdl_keydown (void)
{
	sdlgui.selected = MIN(sdlgui.selected + 1, sdlgui.nitems - 1);
	sdl_gui_update();
	return 0;
}

static int sdl_keyspace (void)
{
	int i;
	int nitems;
	sdl_item_t *item;
	sdl_item_t **items;
	item = sdlgui.items[sdlgui.selected];
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

		for (i = 0; i < sdlgui.nitems; i++) {
			if (sdlgui.items[i] == item) {
				continue;
			}
			if (sdlgui.items[i]->uninit != NULL) {
				sdlgui.items[i]->uninit(sdlgui.items[i]);
			}
		}

		sdlgui.prevs = (sdl_item_t **) realloc(sdlgui.prevs, sizeof(sdl_item_t *) * (sdlgui.nprevs + 1));
		sdlgui.pselected = (int *) realloc(sdlgui.pselected, sizeof(int) * (sdlgui.nprevs + 1));
		sdlgui.prevs[sdlgui.nprevs] = item;
		sdlgui.pselected[sdlgui.nprevs] = sdlgui.selected;
		sdlgui.nprevs++;

		free(sdlgui.items);
		sdlgui.items = items;
		sdlgui.nitems = nitems;
		sdlgui.selected = 0;

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

	if (sdlgui.nprevs <= 1) {
		sdl_gui_uninit();
		sdl_gui_init();
		return 0;
	}

	item = sdlgui.prevs[sdlgui.nprevs - 2];
	selected = sdlgui.pselected[sdlgui.nprevs - 1];

	if (item->items(item, &nitems, &items) != 0) {
		return -1;
	}
	if (nitems <= 0 || items == NULL) {
		return -1;
	}

	for (i = 0; i < sdlgui.nitems; i++) {
		if (sdlgui.items[i]->uninit != NULL) {
			sdlgui.items[i]->uninit(sdlgui.items[i]);
		}
	}
	free(sdlgui.items);
	sdlgui.items = items;
	sdlgui.nitems = nitems;
	sdlgui.selected = selected;

	item = sdlgui.prevs[sdlgui.nprevs - 1];
	if (item->uninit != NULL) {
		item->uninit(item);
	}
	sdlgui.prevs = (sdl_item_t **) realloc(sdlgui.prevs, sizeof(sdl_item_t *) * (sdlgui.nprevs - 1));
	sdlgui.pselected = (int *) realloc(sdlgui.pselected, sizeof(sdl_item_t *) * (sdlgui.nprevs - 1));
	sdlgui.nprevs--;

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
	memset(&sdlgui, 0, sizeof(sdlgui_t));

	sdlgui.width = 720;
	sdlgui.height = 576;
	sdlgui.bitsperpixel = 32;
	sdlgui.pitch = sdlgui.width * ((sdlgui.bitsperpixel + 7) / 8);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		printf("SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
		return -1;
	}

	sdlgui.screen = SDL_SetVideoMode(sdlgui.width, sdlgui.height, sdlgui.bitsperpixel, SDL_HWSURFACE | SDL_SWSURFACE);
	if (sdlgui.screen == NULL) {
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

	sdlgui.surface_data = (unsigned char *) malloc(sdlgui.height * sdlgui.pitch);
	memset(sdlgui.surface_data, 0x00, sdlgui.height * sdlgui.pitch);
	sdlgui.sdl_surface = SDL_CreateRGBSurfaceFrom(sdlgui.surface_data, sdlgui.width, sdlgui.height, sdlgui.bitsperpixel, sdlgui.pitch, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	sdlgui.cairo_surface = cairo_image_surface_create_for_data(sdlgui.surface_data, CAIRO_FORMAT_ARGB32, sdlgui.width, sdlgui.height, sdlgui.pitch);
	sdlgui.cairo_cairo = cairo_create(sdlgui.cairo_surface);
	cairo_set_operator(sdlgui.cairo_cairo, CAIRO_OPERATOR_OVER);

	sdlgui.headgui = cairogui_create_head(370, 50, 340, 50);
	sdlgui.listgui = cairogui_create_list(370, 420, 340, 110);
	sdlgui.buttongui = cairogui_create_button(400, 58, 320, 0, 20);
	sdlgui.previewgui = cairogui_create_preview(300, 420, 10, 110);

	sdl_gui_init();

	sdl_loop();

	sdl_gui_uninit();

	cairogui_destroy(sdlgui.previewgui);
	cairogui_destroy(sdlgui.buttongui);
	cairogui_destroy(sdlgui.listgui);
	cairogui_destroy(sdlgui.headgui);

	cairo_surface_destroy(sdlgui.cairo_surface);
	cairo_destroy(sdlgui.cairo_cairo);
	SDL_FreeSurface(sdlgui.sdl_surface);
	free(sdlgui.surface_data);

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
