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
	char *d;
	char *o;
	entry_t *e;
	entry_t *r;
	client_device_t *device;
	debugfs("enter");
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	if (strcmp(path, "/") == 0) {
		pthread_mutex_lock(&priv.controller->mutex);
		if (list_count(&priv.controller->devices) > 0) {
			filler(buffer, ".metadata", NULL, 0);
		}
		list_for_each_entry(device, &priv.controller->devices, head) {
			filler(buffer, device->name, NULL, 0);
		}
		pthread_mutex_unlock(&priv.controller->mutex);
	} else if (strstr(path, "/.metadata") != NULL) {
	} else {
		if (do_findpath(path, &d, &o) != 0) {
			debugfs("do_findpath() failed");
			return 0;
		}
		e = controller_browse_children(priv.controller, d, o);
		if (e == NULL) {
			debugfs("controller_browse_children('%s', '%s') failed", d, o);
			free(d);
			free(o);
			return 0;
		}
		filler(buffer, ".metadata", NULL, 0);
		r = e;
		while (e) {
			filler(buffer, e->didl.dc.title, NULL, 0);
			e = e->next;
		}
		entry_uninit(r);
		free(d);
		free(o);
	}
	debugfs("leave");
	return 0;
}
