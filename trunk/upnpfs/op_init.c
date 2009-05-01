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

void * op_init (struct fuse_conn_info *conn)
{
	debugfs("enter");
	priv.name = opts.device;
	priv.options = strdup("daemonize=0,interface=172.16.9.146");
	priv.controller = controller_init(priv.options);
	if (priv.controller == NULL) {
		debugfs("controller_init('%s') failed", priv.options);
		exit(-1);
	}
	debugfs("leave");
	return NULL;
}
