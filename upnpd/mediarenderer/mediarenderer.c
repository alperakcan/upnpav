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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "platform.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"
#include "metadata.h"

#include "mediarenderer.h"

struct upnpavd_mediarenderer_s {
	char *interface;
	char *ipaddr;
	char *netmask;
	char *friendlyname;

	device_t *device;
};

upnpavd_mediarenderer_t * upnpavd_mediarenderer_init (const char *interface, const char *friendlyname)
{
	char *opt;
	upnpavd_mediarenderer_t *c;

	c = (upnpavd_mediarenderer_t *) malloc(sizeof(upnpavd_mediarenderer_t));
	if (c == NULL) {
		goto err0;
	}
	memset(c, 0, sizeof(upnpavd_mediarenderer_t));

	c->interface = strdup(interface);
	c->ipaddr = upnpd_interface_getaddr(interface);
	c->netmask = upnpd_interface_getmask(interface);
	c->friendlyname = (friendlyname == NULL) ? strdup("mediarenderer") : strdup(friendlyname);

	if (c->interface == NULL ||
	    c->ipaddr == NULL ||
	    c->netmask == NULL ||
	    c->friendlyname == NULL) {
		goto err1;
	}

	if (asprintf(&opt, "ipaddr=%s,netmask=%s,friendlyname=%s", c->ipaddr, c->netmask, c->friendlyname) < 0) {
		goto err1;
	}

	c->device = upnpd_mediarenderer_init(opt);
	if (c->device == NULL) {
		goto err2;
	}

	free(opt);
	return c;

err2:	free(opt);
err1:	free(c->netmask);
	free(c->ipaddr);
	free(c->interface);
	free(c->friendlyname);
	free(c);
err0:	return NULL;
}

int upnpavd_mediarenderer_uninit (upnpavd_mediarenderer_t *mediarenderer)
{
	free(mediarenderer->interface);
	free(mediarenderer->ipaddr);
	free(mediarenderer->netmask);
	free(mediarenderer->friendlyname);
	upnpd_mediarenderer_uninit(mediarenderer->device);
	free(mediarenderer);
	return 0;
}
