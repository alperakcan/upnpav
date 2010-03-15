/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 - 2010 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 - 2010 CoreCodec, Inc., http://www.CoreCodec.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Any non-LGPL usage of this software or parts of this software is strictly
 * forbidden.
 *
 * Commercial non-LGPL licensing of this software is possible.
 * For more info contact CoreCodec through info@corecodec.com
 */

#include "upnpfs.h"

static inline char * safe_strdup (const char *str)
{
	if (str != NULL) {
		return strdup(str);
	}
	return NULL;
}

static inline int do_validatecache (upnpfs_cache_t *cache)
{
	if (cache->path == NULL ||
	    cache->device == NULL ||
	    cache->object == NULL ||
	    (cache->container == 0 && cache->source == NULL)) {
		return -1;
	}
	return 0;
}

int do_releasecache (upnpfs_cache_t *cache)
{
	if (cache == NULL) {
		return 0;
	}
	free(cache->path);
	free(cache->device);
	free(cache->object);
	free(cache->source);
	free(cache);
	return 0;
}

static upnpfs_cache_t * do_referencecache (upnpfs_cache_t *cache)
{
	upnpfs_cache_t *c;
	c = (upnpfs_cache_t *) malloc(sizeof(upnpfs_cache_t));
	if (c == NULL) {
		return NULL;
	}
	c->path = safe_strdup(cache->path);
	c->device = safe_strdup(cache->device);
	c->object = safe_strdup(cache->object);
	c->source = safe_strdup(cache->source);
	c->container = cache->container;
	c->size = cache->size;
	if (do_validatecache(c) != 0) {
		do_releasecache(c);
		return NULL;
	}
	return c;
}

upnpfs_cache_t * do_findcache (const char *path)
{
	upnpfs_cache_t *c;
	upnpfs_cache_t *n;
	debugfs("enter");
	upnpd_thread_mutex_lock(priv.cache_mutex);
	list_for_each_entry(c, &priv.cache, head) {
		if (strcmp(path, c->path) == 0) {
			n = do_referencecache(c);
			debugfs("returning cache entry %p", n);
			upnpd_thread_mutex_unlock(priv.cache_mutex);
			return n;
		}
	}
	upnpd_thread_mutex_unlock(priv.cache_mutex);
	debugfs("leave");
	return NULL;
}

upnpfs_cache_t * do_insertcache (const char *path, const char *device, entry_t *entry)
{
	upnpfs_cache_t *c;
	upnpfs_cache_t *n;
	debugfs("enter");
	upnpd_thread_mutex_lock(priv.cache_mutex);
	list_for_each_entry(c, &priv.cache, head) {
		if (strcmp(path, c->path) == 0) {
			n = do_referencecache(c);
			debugfs("returning cache entry '%p'", n);
			upnpd_thread_mutex_unlock(priv.cache_mutex);
			return n;
		}
	}
	if (list_count(&priv.cache) > priv.cache_size) {
		c = list_entry(priv.cache.prev, upnpfs_cache_t, head);
		list_del(&c->head);
		debugfs("too many cache, releasing: %s", c->path);
		do_releasecache(c);
	}
	debugfs("creating new cache entry for '%s' '%s : %s", path, device, entry->didl.entryid);
	c = (upnpfs_cache_t *) malloc(sizeof(upnpfs_cache_t));
	if (c == NULL) {
		debugfs("malloc() failed");
		upnpd_thread_mutex_unlock(priv.cache_mutex);
		return NULL;
	}
	memset(c, 0, sizeof(upnpfs_cache_t));
	c->path = safe_strdup(path);
	c->device = safe_strdup(device);
	c->object = safe_strdup(entry->didl.entryid);
	if (strncmp(entry->didl.upnp.object.class, "object.container", strlen("object.container")) == 0) {
		debugfs("cache entry is a container");
		c->container = 1;
	} else if (strncmp(entry->didl.upnp.object.class, "object.item", strlen("object.item")) == 0) {
		debugfs("cache entry is an item");
		c->container = 0;
		c->source = safe_strdup(entry->didl.res.path);
		c->size = entry->didl.res.size;
	}
	if (do_validatecache(c) != 0) {
		debugfs("do_validatecache() failed");
		do_releasecache(c);
		upnpd_thread_mutex_unlock(priv.cache_mutex);
		return NULL;
	}
	n = do_referencecache(c);
	list_add(&c->head, &priv.cache);
	upnpd_thread_mutex_unlock(priv.cache_mutex);
	debugfs("leave");
	return n;
}

upnpfs_cache_t * do_findpath (const char *path)
{
	char *d;
	char *o;
	char *p;
	char *pt;
	char *ptm;
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
	e = upnpd_controller_browse_metadata(priv.controller, d, "0");
	if (e == NULL) {
		free(tmp);
		return NULL;
	}
	if (asprintf(&pt, "/%s", d) < 0) {
		free(tmp);
		upnpd_entry_uninit(e);
		return NULL;
	}
	c = do_insertcache(pt, d, e);
	while (p && *p && (dir = strsep(&p, "/"))) {
		debugfs("looking for '%s", dir);
		free(o);
		o = e->didl.entryid;
		e->didl.entryid = NULL;
		upnpd_entry_uninit(e);
		e = upnpd_controller_browse_children(priv.controller, d, o);
		debugfs("controller_browse_clidren returned %p", e);
		if (e == NULL) {
			debugfs("upnpd_controller_browse_children('%s', '%s') failed", d, o);
			free(pt);
			free(o);
			free(tmp);
			do_releasecache(c);
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
			free(pt);
			free(o);
			upnpd_entry_uninit(r);
			do_releasecache(c);
			free(tmp);
			return NULL;
		}
		e = upnpd_controller_browse_metadata(priv.controller, d, e->didl.entryid);
		if (e == NULL) {
			debugfs("could not find object '%s' in '%s'", o, d);
			free(pt);
			free(o);
			upnpd_entry_uninit(r);
			do_releasecache(c);
			free(tmp);
			return NULL;
		}
		if (asprintf(&ptm, "%s/%s", pt, dir) < 0) {
			free(pt);
			free(o);
			free(tmp);
			upnpd_entry_uninit(r);
			do_releasecache(c);
			return NULL;
		}
		free(pt);
		pt = ptm;
		do_releasecache(c);
		c = do_insertcache(pt, d, e);
		upnpd_entry_uninit(r);
	}
	free(pt);
	free(o);
	upnpd_entry_uninit(e);
	free(tmp);
	debugfs("leave");
	return c;
}
