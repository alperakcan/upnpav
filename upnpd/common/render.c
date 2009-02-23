/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 by Alper Akcan
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

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "common.h"

#if defined(ENABLE_RENDER_FFMPEG)
extern render_module_t render_ffmpeg;
#endif
#if defined(ENABLE_RENDER_GSTREAMER)
extern render_module_t render_gstreamer;
#endif

render_module_t *render_modules[] = {
#if defined(ENABLE_RENDER_FFMPEG)
	&render_ffmpeg,
#endif
#if defined(ENABLE_RENDER_GSTREAMER)
	&render_gstreamer,
#endif
	NULL
};

render_t * render_init (char *name, char *options)
{
	render_t *r;
	render_module_t **m;
	if (name == NULL) {
		debugf("name == NULL");
		return NULL;
	}
	for (m = render_modules; *m; m++) {
		if (strcmp((*m)->name, name) == 0) {
			debugf("selected rendering module '%s'", (*m)->name);
			goto found;
		}
	}
	debugf("no render module found with name '%s'", name);
	return NULL;
found:
	r = (render_t *) malloc(sizeof(render_t));
	if (r == NULL) {
		debugf("malloc(sizeof(render_t)) failed");
		return NULL;
	}
	memset(r, 0, sizeof(render_t));
	r->module = (*m);
	r->module->render = r;
	list_init(&r->mimetypes);
	if (r->module->init(r->module, options) != 0) {
		debugf("r->module->init() failed for '%s'", r->module->name);
		free(r);
		return NULL;
	}
	debugf("initialized rendering module '%s'", r->module->name);
	return r;
}

int render_uninit (render_t *render)
{
	render_mimetype_t *rendermimetype;
	render_mimetype_t *rendermimetypenext;
	if (render == NULL) {
		goto out;
	}
	list_for_each_entry_safe(rendermimetype, rendermimetypenext, &render->mimetypes, head) {
		list_del(&rendermimetype->head);
		free(rendermimetype->mimetype);
		free(rendermimetype);
	}
	render->module->uninit(render->module);
	free(render);
out:	debugf("uninitialized rendering module");
	return 0;
}

int render_register_mimetype (render_t *render, const char *mimetype)
{
	render_mimetype_t *mt;
	if (mimetype == NULL) {
		debugf("mimetype == NULL");
		return -1;
	}
	debugf("registeringmimetype '%s'", mimetype);
	mt = (render_mimetype_t *) malloc(sizeof(render_mimetype_t));
	if (mt == NULL) {
		debugf("malloc(sizeof(render_mimetype_t)) failed");
		return -1;
	}
	memset(mt, 0, sizeof(render_mimetype_t));
	mt->mimetype = strdup(mimetype);
	if (mt->mimetype == NULL) {
		debugf("strdup(%s) failed", mimetype);
		free(mt);
		return -1;
	}
	list_add(&mt->head, &render->mimetypes);
	return 0;
}

int render_play (render_t *render, char *uri)
{
	if (render && render->module && render->module->play) {
		return render->module->play(render->module, uri);
	}
	debugf("cannot play %p", render);
	return -1;
}

int render_pause (render_t *render)
{
	if (render && render->module && render->module->pause) {
		return render->module->pause(render->module);
	}
	debugf("cannot pause");
	return -1;
}

int render_stop (render_t *render)
{
	if (render && render->module && render->module->stop) {
		return render->module->stop(render->module);
	}
	debugf("cannot stop");
	return -1;
}

int render_seek (render_t *render, long offset)
{
	if (render && render->module && render->module->seek) {
		return render->module->seek(render->module, offset);
	}
	debugf("cannot seek");
	return -1;
}

int render_info (render_t *render, render_info_t *info)
{
	memset(info, 0, sizeof(render_info_t));
	if (render && render->module && render->module->info && info) {
		return render->module->info(render->module, info);
	}
	debugf("cannot info");
	return -1;
}
