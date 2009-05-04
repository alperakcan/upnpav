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

int op_open (const char *path, struct fuse_file_info *fi)
{
	char *t;
	upnpfs_file_t *f;
	upnpfs_cache_t *c;
	debugfs("enter");
	t = strstr(path, "/.metadata/");
	if (t != NULL) {
		debugfs("looking for metadata information");
		return -ENOENT;
	} else {
		c = do_findpath(path);
		if (c == NULL) {
			debugfs("do_findpath('%s') failed", path);
			return -ENOENT;
		}
		f = (upnpfs_file_t *) malloc(sizeof(upnpfs_file_t));
		if (f == NULL) {
			return -ENOENT;
		}
		memset(f, 0, sizeof(upnpfs_file_t));
		f->device = strdup(c->device);
		f->object = strdup(c->object);
		f->path = strdup(c->source);
		f->size = c->size;
		fi->fh = (unsigned long) f;
	}
	debugfs("leave");
	return 0;
}
