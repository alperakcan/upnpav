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

#include "icon/mediaserver/48x48x24.h"
#include "icon/mediaserver/48x48x32.h"

typedef enum {
	OPT_INTERFACE    = 0,
	OPT_NETMASK      = 1,
	OPT_DIRECTORY    = 2,
	OPT_CACHED       = 3,
	OPT_FRIENDLYNAME = 4,
	OPT_DAEMONIZE    = 5,
	OPT_UUID         = 6,
	OPT_HELP         = 7,
} mediaserver_options_t;

static char *mediaserver_options[] = {
	"interface",
	"netmask",
	"directory",
	"cached",
	"friendlyname",
	"daemonize",
	"uuid",
	"help",
	NULL,
};

static int mediaserver_help (void)
{
	printf("mediaserver options;\n"
	       "\tinterface=<interface ip>\n"
	       "\tnetmask=<netmask>\n"
	       "\tcached=<cached>\n"
	       "\tdirectory=<content directory service directory>\n"
	       "\tfriendlyname=<device friendlyname>\n"
	       "\tdaemonize=<1, 0>\n"
	       "\tuuid=<xxxx>\n"
	       "\thelp\n");
	return 0;
}

static icon_t *mediaserver_icons[] = {
	& (icon_t) {
		icon_mediaserver_48x48x32_width,
		icon_mediaserver_48x48x32_height,
		icon_mediaserver_48x48x32_depth,
		icon_mediaserver_48x48x32_path,
		icon_mediaserver_48x48x32_mime,
		icon_mediaserver_48x48x32_size,
		(unsigned char *) icon_mediaserver_48x48x32_data
	},
	& (icon_t) {
		icon_mediaserver_48x48x24_width,
		icon_mediaserver_48x48x24_height,
		icon_mediaserver_48x48x24_depth,
		icon_mediaserver_48x48x24_path,
		icon_mediaserver_48x48x24_mime,
		icon_mediaserver_48x48x24_size,
		(unsigned char *) icon_mediaserver_48x48x24_data
	},
	NULL,
};

device_t * mediaserver_init (char *options)
{
	int rc;
	int err;
	int cached;
	char *value;
	int daemonize;
	char *uuid;
	char *netmask;
	char *directory;
	char *interface;
	char *friendlyname;
	char *suboptions;
	device_t *device;
	device_service_t *service;

	debugf("processing device options '%s'", options);

	err = 0;
	cached = 0;
	daemonize = 0;
	uuid = NULL;
	netmask = NULL;
	interface = NULL;
	directory = NULL;
	friendlyname = NULL;
	suboptions = options;
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, mediaserver_options, &value)) {
			case OPT_INTERFACE:
				if (value == NULL) {
					debugf("value is missing for interface option");
					err = 1;
					continue;
				}
				interface = value;
				break;
			case OPT_NETMASK:
				if (value == NULL) {
					debugf("value is missing for netmask option");
					err = 1;
					continue;
				}
				netmask = value;
				break;
			case OPT_DIRECTORY:
				if (value == NULL) {
					debugf("value is missing for directory option");
					err = 1;
					continue;
				}
				directory = value;
				break;
			case OPT_CACHED:
				if (value == NULL) {
					debugf("value is missinf for cached option");
					err = 1;
					continue;
				}
				cached = atoi(value);
				break;
			case OPT_FRIENDLYNAME:
				if (value == NULL) {
					debugf("value is missing for firendlyname option");
					err = 1;
					continue;
				}
				friendlyname = value;
				break;
			case OPT_DAEMONIZE:
				if (value == NULL) {
					debugf("value is missing for daemonize option");
					err = 1;
					continue;
				}
				daemonize = atoi(value);
				break;
			case OPT_UUID:
				if (value == NULL) {
					debugf("value is missing for uuid option");
					err = 1;
					continue;
				}
				uuid = value;
				break;
			default:
			case OPT_HELP:
				mediaserver_help();
				return NULL;
		}
	}

	debugf("starting mediaserver;\n"
	       "\tuuid        : %s\n"
	       "\tdaemonize   : %s\n"
	       "\tinterface   : %s\n"
	       "\tnetmask     : %s\n"
	       "\tdirectory   : %s\n"
	       "\tcached      : %d\n"
	       "\tfriendlyname: %s\n",
	       (uuid) ? uuid : "(default)",
	       (daemonize) ? "yes" : "no",
	       (interface) ? interface : "null",
	       (netmask) ? netmask : "null",
	       (directory) ? directory : "null",
	       cached,
	       (friendlyname) ? friendlyname : "mediaserver");

	debugf("initializing mediaserver device struct");
	device = (device_t *) malloc(sizeof(device_t));
	if (device == NULL) {
		debugf("device = (device_t *) malloc(sizeof(device_t)) failed");
		goto error;
	}
	memset(device, 0, sizeof(device_t));
	device->name = "mediaserver";
	device->interface = interface;
	device->ifmask = netmask;
	device->devicetype = "urn:schemas-upnp-org:device:MediaServer:1";
	device->friendlyname = (friendlyname) ? friendlyname : "mediaserver";
	device->manufacturer = "Alper Akcan";
	device->manufacturerurl = "http://alperakcan.org";
	device->modeldescription = PACKAGE_STRING;
	device->modelname = PACKAGE_NAME;
	device->modelnumber = PACKAGE_VERSION;
	device->modelurl = "http://alperakcan.org/projects/upnpavd";
	device->serialnumber = "1";
	device->upc = "";
	device->presentationurl = "";
	device->icons = mediaserver_icons;
	device->daemonize = daemonize;
	device->uuid = uuid;

	service = contentdirectory_init(directory, cached);
	if (service == NULL) {
		debugf("contendirectory_init() failed");
		goto error;
	}
	if ((rc = device_service_add(device, service)) != 0) {
		debugf("device_service_add(device, service) failed");
		goto error;
	}
	service = connectionmanager_init();
	if (service == NULL) {
		debugf("connectionmanager_init() failed");
		goto error;
	}
	connectionmanager_register_mimetype(service, "*");

	if ((rc = device_service_add(device, service)) != 0) {
		debugf("device_service_add(device, service) failed");
		goto error;
	}

	service = registrar_init();
	if (service == NULL) {
		debugf("registrar_init() failed");
		goto error;
	}
	if ((rc = device_service_add(device, service)) != 0) {
		debugf("device_service_add(device, service) failed");
		goto error;
	}

	debugf("initializing mediaserver device");
	if ((rc = device_init(device)) != 0) {
		debugf("device_init(device) failed");
		goto error;
	}

	debugf("initialized mediaserver device");
	return device;
error:	mediaserver_uninit(device);
	return NULL;
}

int mediaserver_refresh (device_t *mediaserver)
{
	debugf("refreshing content directory service");
	return 0;
}

int mediaserver_uninit (device_t *mediaserver)
{
	debugf("uninitializing mediaserver");
	device_uninit(mediaserver);
	free(mediaserver);
	debugf("uninitialized mediaserver");
	return 0;
}
