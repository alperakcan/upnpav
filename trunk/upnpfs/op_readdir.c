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

#include <pthread.h>

int op_readdir (const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	char *p;
	char *t;
	entry_t *e;
	entry_t *r;
	upnpfs_cache_t *c;
	upnpfs_cache_t *n;
	client_device_t *device;
	debugfs("enter");
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	if (strcmp(path, "/") == 0) {
		pthread_mutex_lock(&priv.controller->mutex);
		if (list_count(&priv.controller->devices) > 0) {
			filler(buffer, ".devices", NULL, 0);
		}
		list_for_each_entry(device, &priv.controller->devices, head) {
			filler(buffer, device->name, NULL, 0);
		}
		pthread_mutex_unlock(&priv.controller->mutex);
	} else if (((t = strstr(path, "/.metadata")) != NULL) &&
		   (strcmp(t, "/.metadata") == 0)) {
		p = strdup(path);
		if (p == NULL) {
			debugfs("strdup() failed");
			return 0;
		}
		t = strstr(p, "/.metadata");
		*t = '\0';
		c = do_findpath(p);
		if (c == NULL) {
			debugfs("do_findpath() failed");
			free(p);
			return 0;
		}
		free(p);
		e = controller_browse_children(priv.controller, c->device, c->object);
		if (e == NULL) {
			debugfs("controller_browse_children('%s', '%s') failed", c->device, c->object);
			do_releasecache(c);
			return 0;
		}
		r = e;
		while (e) {
			p = NULL;
			if (asprintf(&p, "%s.xml", e->didl.dc.title) >= 0) {
				filler(buffer, p, NULL, 0);
				free(p);
			}
			e = e->next;
		}
		do_releasecache(c);
		entry_uninit(r);
	} else {
		c = do_findpath(path);
		if (c == NULL) {
			debugfs("do_findpath() failed");
			return 0;
		}
		debugfs("cache entry for '%s' is '%s : %s'", path, c->device, c->object);
		e = controller_browse_children(priv.controller, c->device, c->object);
		if (e == NULL) {
			debugfs("controller_browse_children('%s', '%s') failed", c->device, c->object);
			do_releasecache(c);
			return 0;
		}
		filler(buffer, ".metadata", NULL, 0);
		r = e;
		while (e) {
			char *pt;
			if (asprintf(&pt, "%s/%s", path, e->didl.dc.title) >= 0) {
				n = do_insertcache(pt, c->device, e);
				do_releasecache(n);
				free(pt);
			}
			filler(buffer, e->didl.dc.title, NULL, 0);
			e = e->next;
		}
		do_releasecache(c);
		entry_uninit(r);
	}
	debugfs("leave");
	return 0;
}
