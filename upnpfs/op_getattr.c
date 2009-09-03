/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 CoreCodec, Inc., http://www.CoreCodec.com
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

int op_getattr (const char *path, struct stat *stbuf)
{
	char *p;
	time_t t;
	upnpfs_cache_t *c;
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
	} else if ((p = strstr(path, "/.devices")) != NULL) {
		if (strcmp(p, "/.devices") == 0) {
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
			stbuf->st_size = 65536;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		}
	} else {
		c = do_findpath(path);
		if (c == NULL) {
			return -ENOENT;
		}
		if (c->container == 1) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			stbuf->st_size = 512;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		} else {
			stbuf->st_mode = S_IFREG | 0444;
			stbuf->st_nlink = 1;
			stbuf->st_size = c->size;
			stbuf->st_atime = t;
			stbuf->st_mtime = t;
			stbuf->st_ctime = t;
		}
		do_releasecache(c);
	}
	debugfs("leave");
	return 0;
}
