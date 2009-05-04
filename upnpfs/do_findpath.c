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

upnpfs_cache_t * do_findcache (const char *path)
{
	upnpfs_cache_t *c;
	debugfs("enter");
	list_for_each_entry(c, &priv.cache, head) {
		if (strcmp(path, c->path) == 0) {
			debugfs("returning cache entry %p", c);
			return c;
		}
	}
	debugfs("leave");
	return NULL;
}

upnpfs_cache_t * do_insertcache (const char *path, const char *device, entry_t *entry)
{
	upnpfs_cache_t *c;
	debugfs("enter");
	list_for_each_entry(c, &priv.cache, head) {
		if (strcmp(path, c->path) == 0) {
			debugfs("returning cache entry %p", c);
			return c;
		}
	}
	if (list_count(&priv.cache) > priv.cache_size) {
		c = list_entry(priv.cache.prev, upnpfs_cache_t, head);
		list_del(&c->head);
		free(c->path);
		free(c->device);
		free(c->object);
		free(c->source);
		free(c);
	}
	debugfs("creating new cache entry");
	c = (upnpfs_cache_t *) malloc(sizeof(upnpfs_cache_t));
	if (c == NULL) {
		debugfs("malloc() failed");
		return NULL;
	}
	memset(c, 0, sizeof(upnpfs_cache_t));
	debugfs("path: %s", path);
	c->path = strdup(path);
	debugfs("device: %s", device);
	c->device = strdup(device);
	debugfs("title: %s", entry->didl.dc.title);
	c->object = strdup(entry->didl.entryid);
	if (strncmp(entry->didl.upnp.object.class, "object.container", strlen("object.container")) == 0) {
		debugfs("cache entry is a container");
		c->container = 1;
	} else if (strncmp(entry->didl.upnp.object.class, "object.item", strlen("object.item")) == 0) {
		debugfs("cache entry is an item");
		c->container = 0;
		c->source = strdup(entry->didl.res.path);
		c->size = entry->didl.res.size;
	}
	list_add(&c->head, &priv.cache);
	debugfs("leave");
	return c;
}

upnpfs_cache_t * do_findpath (const char *path)
{
	char *d;
	char *o;
	char *p;
	char *pt;
	char *dir;
	char *tmp;
	entry_t *e;
	entry_t *r;
	upnpfs_cache_t *c;
	debugfs("path: %s", path);
	if (strstr(path, "/.") != NULL) {
		debugfs("hidden file ?");
		return NULL;
	}
	c = do_findcache(path);
	if (c != NULL) {
		debugfs("working from cache");
		return c;
	}
	tmp  = strdup(path);
	if  (tmp == NULL) {
		return NULL;
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
		return NULL;
	}
	debugfs("device: '%s'", d);
	e = controller_browse_metadata(priv.controller, d, "0");
	if (e == NULL) {
		free(tmp);
		return NULL;
	}
	if (asprintf(&pt, "/%s", d) < 0) {
		free(tmp);
		entry_uninit(e);
		return NULL;
	}
	c = do_insertcache(pt, d, e);
	free(pt);
	while (p && *p && (dir = strsep(&p, "/"))) {
		debugfs("looking for '%s", dir);
		free(o);
		o = e->didl.entryid;
		e->didl.entryid = NULL;
		entry_uninit(e);
		e = controller_browse_children(priv.controller, d, o);
		debugfs("controller_browse_clidren returned %p", e);
		if (e == NULL) {
			debugfs("controller_browse_children('%s', '%s') failed", d, o);
			free(o);
			free(tmp);
			return NULL;
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
			return NULL;
		}
		e = controller_browse_metadata(priv.controller, d, e->didl.entryid);
		if (e == NULL) {
			debugfs("could not find object '%s' in '%s'", o, d);
			free(o);
			entry_uninit(r);
			free(tmp);
			return NULL;
		}
		c = do_insertcache(path, d, e);
		entry_uninit(r);
	}
	free(o);
	entry_uninit(e);
	free(tmp);
	debugfs("leave");
	return c;
}
