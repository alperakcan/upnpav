/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2010 Alper Akcan, alper.akcan@gmail.com
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

static inline upnpfs_cache_t * do_findmetadata (const char *path)
{
	char *p;
	char *t;
	char *m;
	upnpfs_cache_t *c;
	p = strdup(path);
	if (p == NULL) {
		return NULL;
	}
	t = p + strlen(p) - strlen(".xml");
	if (strcmp(t, ".xml") != 0) {
		free(p);
		return NULL;
	}
	*t = '\0';
	t = strstr(p, "/.metadata/");
	if (t == NULL) {
		free(p);
		return NULL;
	}
	m = t + strlen("/.metadata");
	memmove(t, m, strlen(m) + 1);
	debugfs("path from metadata is '%s'", p);
	c = do_findpath(p);
	free(p);
	return c;
}

int op_open (const char *path, struct fuse_file_info *fi)
{
	char *t;
	upnpfs_file_t *f;
	upnpfs_cache_t *c;
	debugfs("enter");
	f = (upnpfs_file_t *) malloc(sizeof(upnpfs_file_t));
	if (f == NULL) {
		return -EIO;
	}
	memset(f, 0, sizeof(upnpfs_file_t));
	t = strstr(path, "/.metadata/");
	if (t != NULL) {
		debugfs("looking for metadata information");
		c = do_findmetadata(path);
		if (c == NULL) {
			debugfs("do_findmetadata('%s') failed", path);
			return -ENOENT;
		}
		f->metadata = 1;
		f->cache = c;
		fi->fh = (unsigned long) f;
	} else {
		c = do_findpath(path);
		if (c == NULL) {
			debugfs("do_findpath('%s') failed", path);
			return -ENOENT;
		}
		f->metadata = 0;
		f->cache = c;
		fi->fh = (unsigned long) f;
	}
	debugfs("leave");
	return 0;
}
