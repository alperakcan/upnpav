/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 by Alper Akcan
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

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "common.h"

typedef struct mediarender_device_s {
	/** */
	device_t device;
	/** */
	render_t *render;
} mediarender_device_t;

device_t * mediarender_init (char *options)
{
	int rc;
	device_service_t *service;
	mediarender_device_t *device;
	render_mimetype_t *rendermimetype;

	debugf("initializing mediarender device struct");
	device = (mediarender_device_t *) malloc(sizeof(mediarender_device_t));
	if (device == NULL) {
		debugf("device = (mediarender_device_t *) malloc(sizeof(mediarender_device_t)) failed");
		goto error;
	}
	memset(device, 0, sizeof(mediarender_device_t));
	device->device.name = "mediarender";
	device->device.devicetype = "urn:schemas-upnp-org:device:MediaRenderer:1";
	device->device.friendlyname = "mediarender";
	device->device.manufacturer = "Alper Akcan";
	device->device.manufacturerurl = "http://alperakcan.org";
	device->device.modeldescription = PACKAGE_STRING;
	device->device.modelname = PACKAGE_NAME;
	device->device.modelnumber = PACKAGE_VERSION;
	device->device.modelurl = "http://alperakcan.org/upnpd/mediarender";
	device->device.serialnumber = "1";
	device->device.upc = "";
	device->device.presentationurl = "";

	device->render = render_init("ffmpeg", options);
	if (device->render == NULL) {
		debugf("render_init() failed");
		goto error;
	}

	service = connectionmanager_init();
	if (service == NULL) {
		debugf("connectionmanager_init() failed");
		goto error;
	}
	if ((rc = device_service_add(&device->device, service)) != 0) {
		debugf("device_service_add(device, service) failed");
		goto error;
	}
	list_for_each_entry(rendermimetype, &device->render->mimetypes, head) {
		connectionmanager_register_mimetype(service, rendermimetype->mimetype);
	}
	connectionmanager_register_mimetype(service, "audio/mpeg");
	connectionmanager_register_mimetype(service, "video/x-msvideo");

	service = avtransport_init(device->render);
	if (service == NULL) {
		debugf("avtransport_init() failed");
		goto error;
	}
	if ((rc = device_service_add(&device->device, service)) != 0) {
		debugf("device_service_add(device, service) failed");
		goto error;
	}
	service = renderingcontrol_init();
	if (service == NULL) {
		debugf("renderingcontrol_init() failed");
		goto error;
	}
	if ((rc = device_service_add(&device->device, service)) != 0) {
		debugf("device_service_add(device, service) failed");
		goto error;
	}

	debugf("initializing mediarender device");
	if ((rc = device_init(&device->device)) != 0) {
		debugf("device_init(device) failed");
		goto error;
	}

	debugf("initialized mediarender device");
	return &device->device;
error:	mediarender_uninit(&device->device);
	return NULL;
}

int mediarender_uninit (device_t *device)
{
	mediarender_device_t *mediarender;
	mediarender = (mediarender_device_t *) device;
	if (device == NULL) {
		goto out;
	}
	debugf("uninitializing gstreamer");
	render_uninit(mediarender->render);
	debugf("uninitializing mediarender");
	device_uninit(&mediarender->device);
	free(mediarender);
out:	debugf("uninitialized mediarender");
	return 0;
}
