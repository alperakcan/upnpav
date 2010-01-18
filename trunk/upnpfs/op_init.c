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

void * op_init (struct fuse_conn_info *conn)
{
	char *ipaddr;
	char *netmask;
	debugfs("enter");
	ipaddr = interface_getaddr(opts.interface);
	if (ipaddr == NULL) {
		debugfs("interface_getaddr('%s') failed", opts.interface);
		exit(-1);
	}
	netmask = interface_getmask(opts.interface);
	if (netmask == NULL) {
		debugfs("interface_getmask('%s') failed", opts.interface);
		exit(-2);
	}
	if (asprintf(&priv.options, "daemonize=0,interface=%s,netmask=%s", ipaddr, netmask) < 0) {
		debugfs("interface_getaddr('%s') failed", opts.interface);
		exit(-3);
	}
	priv.controller = controller_init(priv.options);
	if (priv.controller == NULL) {
		debugfs("controller_init('%s') failed", priv.options);
		exit(-4);
	}
	list_init(&priv.cache);
	priv.cache_size = opts.cache_size;
	priv.cache_mutex = thread_mutex_init("cache_mutex", 0);
	free(ipaddr);
	debugfs("leave");
	return NULL;
}
