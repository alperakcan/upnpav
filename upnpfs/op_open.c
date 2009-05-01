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
	char *d;
	char *o;
	entry_t *e;
	upnpfs_file_t *f;
	debugfs("enter");
	if (do_findpath(path, &d, &o) != 0) {
		debugfs("do_findpath('%s') failed", path);
		return -ENOENT;
	}
	e = controller_browse_metadata(priv.controller, d, o);
	if (e == NULL) {
		free(d);
		free(o);
		return -ENOENT;
	}
	if (e->didl.res.path == NULL) {
		free(d);
		free(o);
		entry_uninit(e);
		return -ENOENT;
	}
	f = (upnpfs_file_t *) malloc(sizeof(upnpfs_file_t));
	if (f == NULL) {
		free(d);
		free(o);
		entry_uninit(e);
		return -ENOENT;
	}
	entry_dump(e);
	memset(f, 0, sizeof(upnpfs_file_t));
	f->device = d;
	f->object = o;
	f->path = e->didl.res.path;
	f->size = e->didl.res.size;
	f->protocol = e->didl.res.protocolinfo;
	fi->fh = (unsigned long) f;
	d = NULL;
	o = NULL;
	e->didl.res.path = NULL;
	e->didl.res.protocolinfo = NULL;
	entry_uninit(e);
	debugfs("leave");
	return 0;
}
