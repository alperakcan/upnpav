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

int op_getattr (const char *path, struct stat *stbuf)
{
	char *d;
	char *o;
	char *p;
	time_t t;
	entry_t *e;
	debugfs("enter");
	debugfs("path = '%s'", path);
	t = 0;
	memset(stbuf, 0, sizeof(*stbuf));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_size = 512;
		stbuf->st_atime = t;
		stbuf->st_mtime = t;
		stbuf->st_ctime = t;
	} else if (strcmp(path, "/.devices") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_size = 512;
		stbuf->st_atime = t;
		stbuf->st_mtime = t;
		stbuf->st_ctime = t;
	} else if ((p = strstr(path, "/.metadata")) != NULL) {
		if (strcmp(p, "/.metadata") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			stbuf->st_size = 512;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		} else {
			stbuf->st_mode = S_IFREG | 0444;
			stbuf->st_nlink = 1;
			stbuf->st_size = 0;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		}
	} else {
		if (do_findpath(path, &d, &o) != 0) {
			return -ENOENT;
		}
		e = controller_browse_metadata(priv.controller, d, o);
		if (e == NULL) {
			free(d);
			free(o);
			return -ENOENT;
		}
		if (strncmp(e->didl.upnp.object.class, "object.container", strlen("object.container")) == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			stbuf->st_size = 512;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		} else if (strncmp(e->didl.upnp.object.class, "object.item", strlen("object.item")) == 0) {
			stbuf->st_mode = S_IFREG | 0444;
			stbuf->st_nlink = 1;
			stbuf->st_size = e->didl.res.size;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		} else {
			entry_uninit(e);
			free(d);
			free(o);
			return -ENOENT;
		}
		//entry_dump(e);
		entry_uninit(e);
		free(d);
		free(o);
	}
	debugfs("leave");
	return 0;
}
