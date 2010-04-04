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
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

#include "icon/mediarenderer/48x48x24.h"
#include "icon/mediarenderer/48x48x32.h"

typedef enum {
	OPT_IPADDR       = 0,
	OPT_NETMASK      = 1,
	OPT_FRIENDLYNAME = 2,
	OPT_DAEMONIZE    = 3,
	OPT_UUID         = 4,
	OPT_HELP         = 5,
} mediarenderer_options_t;

static char *mediarenderer_options[] = {
	"ipaddr",
	"netmask",
	"friendlyname",
	"daemonize",
	"uuid",
	"help",
	NULL,
};

static int mediarenderer_help (void)
{
	printf("mediarenderer options;\n"
	       "\tipaddr=<ipaddr>\n"
	       "\tnetmask=<netmask>\n"
	       "\tfriendlyname=<device friendlyname>\n"
	       "\tdaemonize=<1, 0>\n"
	       "\tuuid=<xxxx>\n"
	       "\thelp\n");
	return 0;
}

static icon_t mediarenderer_icons[] = {
	{
		icon_mediarenderer_48x48x32_path,
		icon_mediarenderer_48x48x32_width,
		icon_mediarenderer_48x48x32_height,
		icon_mediarenderer_48x48x32_depth,
		icon_mediarenderer_48x48x32_mime,
		icon_mediarenderer_48x48x32_size,
		(unsigned char *) icon_mediarenderer_48x48x32_data
	},
	{
		icon_mediarenderer_48x48x24_path,
		icon_mediarenderer_48x48x24_width,
		icon_mediarenderer_48x48x24_height,
		icon_mediarenderer_48x48x24_depth,
		icon_mediarenderer_48x48x24_mime,
		icon_mediarenderer_48x48x24_size,
		(unsigned char *) icon_mediarenderer_48x48x24_data
	},
	{
		NULL,
	},
};

device_t * upnpd_mediarenderer_init (char *options)
{
	int rc;
	int err;
	char *uuid;
	char *value;
	int daemonize;
	char *netmask;
	char *ipaddr;
	char *friendlyname;
	char *suboptions;
	device_t *device;
	device_service_t *service;

	debugf(_DBG, "processing device options '%s'", options);

	err = 0;
	daemonize = 0;
	uuid = NULL;
	netmask = NULL;
	ipaddr = NULL;
	friendlyname = NULL;
	suboptions = options;
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, mediarenderer_options, &value)) {
			case OPT_IPADDR:
				if (value == NULL) {
					debugf(_DBG, "value is missing for ipaddr option");
					err = 1;
					continue;
				}
				ipaddr = value;
				break;
			case OPT_NETMASK:
				if (value == NULL) {
					debugf(_DBG, "value is missing for netmask option");
					err = 1;
					continue;
				}
				netmask = value;
				break;
			case OPT_FRIENDLYNAME:
				if (value == NULL) {
					debugf(_DBG, "value is missing for firendlyname option");
					err = 1;
					continue;
				}
				friendlyname = value;
				break;
			case OPT_DAEMONIZE:
				if (value == NULL) {
					debugf(_DBG, "value is missing for daemonize option");
					err = 1;
					continue;
				}
				daemonize = atoi(value);
				break;
			case OPT_UUID:
				if (value == NULL) {
					debugf(_DBG, "value is missing for uuid option");
					err = 1;
					continue;
				}
				uuid = value;
				break;
			default:
				break;
			case OPT_HELP:
				mediarenderer_help();
				return NULL;
		}
	}

	debugf(_DBG, "starting mediarenderer;\n"
	       "\tuuid        : %s\n"
	       "\tdaemonize   : %s\n"
	       "\tipaddr   : %s\n"
	       "\tnetmask     : %s\n"
	       "\tfriendlyname: %s\n",
	       (uuid) ? uuid : "(default)",
	       (daemonize) ? "yes" : "no",
	       (ipaddr) ? ipaddr : "null",
	       (netmask) ? netmask : "null",
	       (friendlyname) ? friendlyname : "mediarenderer");

	debugf(_DBG, "initializing mediarenderer device struct");
	device = (device_t *) malloc(sizeof(device_t));
	if (device == NULL) {
		debugf(_DBG, "device = (device_t *) malloc(sizeof(device_t)) failed");
		goto error;
	}
	memset(device, 0, sizeof(device_t));
	device->name = "mediarenderer";
	device->ipaddr = ipaddr;
	device->ifmask = netmask;
	device->devicetype = "urn:schemas-upnp-org:device:MediaRenderer:1";
	device->friendlyname = (friendlyname) ? friendlyname : "mediarenderer";
	device->manufacturer = "Alper Akcan";
	device->manufacturerurl = "http://alperakcan.org";
	device->modeldescription = PACKAGE_STRING;
	device->modelname = PACKAGE_NAME;
	device->modelnumber = PACKAGE_VERSION;
	device->modelurl = "http://alperakcan.org/projects/upnpavd";
	device->serialnumber = "1";
	device->upc = "";
	device->presentationurl = "";
	device->icons = mediarenderer_icons;
	device->daemonize = daemonize;
	device->uuid = uuid;

	service = upnpd_registrar_init();
	if (service == NULL) {
		debugf(_DBG, "upnpd_registrar_init() failed");
		goto error;
	}
	if ((rc = upnpd_device_service_add(device, service)) != 0) {
		debugf(_DBG, "upnpd_device_service_add(device, service) failed");
		goto error;
	}

	debugf(_DBG, "initializing mediarenderer device");
	if ((rc = upnpd_device_init(device)) != 0) {
		debugf(_DBG, "upnpd_device_init(device) failed");
		goto error;
	}

	debugf(_DBG, "initialized mediarenderer device");
	return device;
error:	upnpd_mediarenderer_uninit(device);
	return NULL;
}

int upnpd_mediarenderer_uninit (device_t *mediarenderer)
{
	debugf(_DBG, "uninitializing mediarenderer");
	upnpd_device_uninit(mediarenderer);
	free(mediarenderer);
	debugf(_DBG, "uninitialized mediarenderer");
	return 0;
}
