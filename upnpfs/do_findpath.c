/**
 * Copyright (c) 2009 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the fuse-ext2
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "upnpfs.h"

static int do_findcache (const char *path, char **device, char **object)
{
	upnpfs_cache_t *c;
	debugfs("enter");
	list_for_each_entry(c, &priv.cache, head) {
		if (strcmp(path, c->path) == 0) {
			*device = strdup(c->device);
			*object = strdup(c->object);
			debugfs("cache found for '%s'", path);
			return 0;
		}
	}
	debugfs("leave");
	return -1;
}

static int do_insertcache (const char *path, const char *device, char *object)
{
	upnpfs_cache_t *c;
	debugfs("enter");
	list_for_each_entry(c, &priv.cache, head) {
		if (strcmp(path, c->path) == 0) {
			free(c->device);
			free(c->object);
			c->device = strdup(device);
			c->object = strdup(object);
			debugfs("cache update for '%s'", path);
			return 0;
		}
	}
	c = (upnpfs_cache_t *) malloc(sizeof(upnpfs_cache_t));
	memset(c, 0, sizeof(upnpfs_cache_t));
	c->path = strdup(path);
	c->device = strdup(device);
	c->object = strdup(object);
	list_add(&c->head, &priv.cache);
	debugfs("insert cache update for '%s'", path);
	debugfs("leave");
	return 0;
}

int do_findpath (const char *path, char **device, char **object)
{
	char *d;
	char *o;
	char *p;
	char *dir;
	char *tmp;
	entry_t *e;
	entry_t *r;
	debugfs("path: %s", path);
	*device = NULL;
	*object = NULL;
	if (do_findcache(path, device, object) == 0) {
		debugfs("working from cache");
		return 0;
	}
	tmp  = strdup(path);
	if  (tmp == NULL) {
		return -1;
	}
	p = tmp;
	d = NULL;
	o = NULL;
	while (p && *p && (dir = strsep(&p, "/"))) {
		if (strlen(dir) == 0) {
			continue;
		}
		if (d == NULL) {
			d = dir;
			break;
		}
	}
	if (d == NULL) {
		free(tmp);
		return -1;
	}
	debugfs("device: '%s'", d);
	e = controller_browse_metadata(priv.controller, d, "0");
	if (e == NULL) {
		free(tmp);
		return -1;
	}
	while (p && *p && (dir = strsep(&p, "/"))) {
		debugfs("looking for '%s", dir);
		free(o);
		o = e->didl.entryid;
		e->didl.entryid = NULL;
		entry_uninit(e);
		e = controller_browse_children(priv.controller, d, o);
		if (e == NULL) {
			debugfs("controller_browse_children('%s', '%s') failed", d, o);
			free(o);
			free(tmp);
			return -1;
		}
		r = e;
		while (e) {
			if (strcmp(dir, e->didl.dc.title) == 0) {
				break;
			}
			e = e->next;
		}
		if (e == NULL) {
			debugfs("could not find object '%s' in '%s'", o, d);
			free(o);
			entry_uninit(r);
			free(tmp);
			return -1;
		}
		e = controller_browse_metadata(priv.controller, d, e->didl.entryid);
		if (e == NULL) {
			debugfs("could not find object '%s' in '%s'", o, d);
			free(o);
			entry_uninit(r);
			free(tmp);
			return -1;
		}
		entry_uninit(r);
	}
	free(o);
	*device = strdup(d);
	*object = strdup(e->didl.entryid);
	if (*device == NULL || *object == NULL) {
		free(*device);
		free(*object);
		*device = NULL;
		*object = NULL;
		entry_uninit(e);
		free(tmp);
		return -1;
	}
	do_insertcache(path, *device, *object);
	entry_uninit(e);
	free(tmp);
	return 0;
}
