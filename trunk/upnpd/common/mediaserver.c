/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 - 2009 by Alper Akcan
    email                : alper.akcan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>

#include "gena.h"
#include "upnp.h"
#include "common.h"

typedef enum {
	OPT_INTERFACE    = 0,
	OPT_DIRECTORY    = 1,
	OPT_CACHED       = 2,
	OPT_FRIENDLYNAME = 3,
	OPT_DAEMONIZE    = 4,
	OPT_UUID         = 5,
	OPT_HELP         = 6,
} mediaserver_options_t;

static char *mediaserver_options[] = {
	[OPT_INTERFACE]    = "interface",
	[OPT_DIRECTORY]    = "directory",
	[OPT_CACHED]       = "cached",
	[OPT_FRIENDLYNAME] = "friendlyname",
	[OPT_DAEMONIZE]    = "daemonize",
	[OPT_UUID]         = "uuid",
	[OPT_HELP]         = "help",
	NULL,
};

static int mediaserver_help (void)
{
	printf("mediaserver options;\n"
	       "\tinterface=<interface ip>\n"
	       "\tcached=<cached>\n"
	       "\tdirectory=<content directory service directory>\n"
	       "\tfriendlyname=<device friendlyname>\n"
	       "\tdaemonize=<1, 0>\n"
	       "\tuuid=<uuid:xxxx>\n"
	       "\thelp\n");
	return 0;
}
device_t * mediaserver_init (char *options)
{
	int rc;
	int err;
	int cached;
	char *value;
	int daemonize;
	char *uuid;
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
	       "\tdirectory   : %s\n"
	       "\tcached      : %d\n"
	       "\tfriendlyname: %s\n",
	       (uuid) ? uuid : "(default)",
	       (daemonize) ? "yes" : "no",
	       (interface) ? interface : "null",
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
	device->icons = NULL;
	device->daemonize = daemonize;

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
	debugf("refreshing contnt directory service");
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
