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

int op_readdir (const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	int i;
	int j;
	int nd;
	char **d;
	char *p;
	char *t;
	char *pt;
	entry_t *e;
	entry_t *r;
	upnpfs_cache_t *c;
	upnpfs_cache_t *n;
	client_device_t *device;
	debugfs("enter");
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	if (strcmp(path, "/") == 0) {
		upnpd_thread_mutex_lock(priv.controller->mutex);
		if (list_count(&priv.controller->devices) <= 0) {
			goto out;
		}
		d = (char **) malloc(sizeof(char *) * list_count(&priv.controller->devices));
		if (d == NULL) {
			upnpd_thread_mutex_unlock(priv.controller->mutex);
			goto out;
		}
		nd = 0;
		list_for_each_entry(device, &priv.controller->devices, head) {
			d[nd] = strdup(device->name);
			if (d[nd] != NULL) {
				nd++;
			}
		}
		upnpd_thread_mutex_unlock(priv.controller->mutex);

		if (nd <= 0) {
			goto out;
		}
		for (i = 0, j = 0; i < nd; i++) {
			e = upnpd_controller_browse_metadata(priv.controller, d[i], "0");
			if (e != NULL) {
				j++;
				filler(buffer, d[i], NULL, 0);
			}
			upnpd_entry_uninit(e);
			free(d[i]);
		}
		free(d);
		if (j > 0) {
			filler(buffer, ".devices", NULL, 0);
		}
	} else if (strcmp(path, "/.devices") == 0) {
		upnpd_thread_mutex_lock(priv.controller->mutex);
		if (list_count(&priv.controller->devices) <= 0) {
			goto out;
		}
		d = (char **) malloc(sizeof(char *) * list_count(&priv.controller->devices));
		if (d == NULL) {
			upnpd_thread_mutex_unlock(priv.controller->mutex);
			goto out;
		}
		nd = 0;
		list_for_each_entry(device, &priv.controller->devices, head) {
			d[nd] = strdup(device->name);
			if (d[nd] != NULL) {
				nd++;
			}
		}
		upnpd_thread_mutex_unlock(priv.controller->mutex);

		if (nd <= 0) {
			goto out;
		}
		for (i = 0, j = 0; i < nd; i++) {
			e = upnpd_controller_browse_metadata(priv.controller, d[i], "0");
			if (e != NULL) {
				j++;
				if (asprintf(&p, "%s.txt", d[i]) > 0) {
					filler(buffer, p, NULL, 0);
					free(p);
				}
			}
			upnpd_entry_uninit(e);
			free(d[i]);
		}
		free(d);
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
		e = upnpd_controller_browse_children(priv.controller, c->device, c->object);
		if (e == NULL) {
			debugfs("upnpd_controller_browse_children('%s', '%s') failed", c->device, c->object);
			do_releasecache(c);
			return 0;
		}
		r = e;
		while (e) {
			p = NULL;
			if (asprintf(&p, "%s.txt", e->didl.dc.title) >= 0) {
				filler(buffer, p, NULL, 0);
				free(p);
			}
			e = e->next;
		}
		do_releasecache(c);
		upnpd_entry_uninit(r);
	} else {
		c = do_findpath(path);
		if (c == NULL) {
			debugfs("do_findpath() failed");
			return 0;
		}
		debugfs("cache entry for '%s' is '%s : %s'", path, c->device, c->object);
		e = upnpd_controller_browse_children(priv.controller, c->device, c->object);
		if (e == NULL) {
			debugfs("upnpd_controller_browse_children('%s', '%s') failed", c->device, c->object);
			do_releasecache(c);
			return 0;
		}
		filler(buffer, ".metadata", NULL, 0);
		r = e;
		while (e) {
			if (asprintf(&pt, "%s/%s", path, e->didl.dc.title) >= 0) {
				n = do_insertcache(pt, c->device, e);
				do_releasecache(n);
				free(pt);
			}
			filler(buffer, e->didl.dc.title, NULL, 0);
			e = e->next;
		}
		do_releasecache(c);
		upnpd_entry_uninit(r);
	}
out:
	debugfs("leave");
	return 0;
}
