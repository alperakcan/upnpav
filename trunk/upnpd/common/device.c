/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
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
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#include "platform.h"
#include "xmlparser.h"
#include "gena.h"
#include "upnp.h"

#include "common.h"
#include "uuid.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static int device_vfsgetinfo (void *cookie, char *path, gena_fileinfo_t *info)
{
	int s;
	device_t *device;
	device_service_t *service;
	debugf("device vfs get info (%s)", path);
	device = (device_t *) cookie;
	thread_mutex_lock(device->mutex);
	/* is fake file */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (strcmp(path, service->scpdurl) == 0) {
			info->size = strlen(service->description);
			info->mtime = 0;
			info->mimetype = strdup("text/xml");
			debugf("found service scpd url (%d)", info->size);
			thread_mutex_unlock(device->mutex);
			return 0;
		}
	}
	/* check services */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (service->vfscallbacks != NULL && service->vfscallbacks->info != NULL) {
			if (service->vfscallbacks->info(service, path, info) == 0) {
				debugf("found service file");
				thread_mutex_unlock(device->mutex);
				return 0;
			}
		}
	}
	thread_mutex_unlock(device->mutex);
	return -1;
}

static void * device_vfsopen (void *cookie, char *path, gena_filemode_t mode)
{
	int s;
	upnp_file_t *file;
	device_t *device;
	device_service_t *service;
	debugf("device vfs open (%s)", path);
	device = (device_t *) cookie;
	thread_mutex_lock(device->mutex);
	/* is fake file */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (strcmp(path, service->scpdurl) == 0) {
			file = (upnp_file_t *) malloc(sizeof(upnp_file_t));
			if (file == NULL) {
				thread_mutex_unlock(device->mutex);
				return NULL;
			}
			memset(file, 0, sizeof(upnp_file_t));
			file->virtual = 1;
			file->size = strlen(service->description);
			file->buf = strdup(service->description);
			thread_mutex_unlock(device->mutex);
			return file;
		}
	}
	/* check services */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (service->vfscallbacks != NULL && service->vfscallbacks->open != NULL) {
			file = service->vfscallbacks->open(service, path, mode);
			if (file != NULL) {
				debugf("found service file");
				thread_mutex_unlock(device->mutex);
				return file;
			}
		}
	}
	thread_mutex_unlock(device->mutex);
	return NULL;
}

static int device_vfsread (void *cookie, void *handle, char *buffer, unsigned int length)
{
	size_t len;
	upnp_file_t *file;
	debugf("device vfs read");
	file = (upnp_file_t *) handle;
	if (file == NULL) {
		return -1;
	}
	/* is fake file */
	if (file->virtual == 1) {
		if (file->offset >= file->size) {
			return -1;
		}
		len = ((file->size - file->offset) < length) ? (file->size - file->offset) : length;
		memcpy(buffer, file->buf + file->offset, len);
		file->offset += len;
		return len;
	}
	/* check services */
	if (file->service != NULL &&
	    file->service->vfscallbacks != NULL &&
	    file->service->vfscallbacks->read != NULL) {
		debugf("calling service read function");
		len = file->service->vfscallbacks->read(file->service, handle, buffer, length);
		debugf("requested len: %d, returned len: %d", length, len);
		return len;
	}
	return -1;
}

static int device_vfswrite (void *cookie, void *handle, char *buffer, unsigned int length)
{
	debugf("device vfs write");
	assert(0);
	return 0;
}

static unsigned long long device_vfsseek (void *cookie, void *handle, unsigned long long offset, gena_seek_t whence)
{
	upnp_file_t *file;
	unsigned long long off;
	debugf("device vfs seek");
	file = (upnp_file_t *) handle;
	if (file == NULL) {
		return -1;
	}
	/* is fake file */
	if (file->virtual == 1) {
		switch (whence) {
			case GENA_SEEK_SET: file->offset = offset; break;
			case GENA_SEEK_CUR: file->offset += offset; break;
			case GENA_SEEK_END: file->offset = file->size + offset; break;
		}
		file->offset = MAX(0, file->offset);
		file->offset = MIN(file->offset, file->size);
		return file->offset;
	}
	/* check services */
	if (file->service != NULL &&
	    file->service->vfscallbacks != NULL &&
	    file->service->vfscallbacks->seek != NULL) {
		debugf("calling service seek function");
		off = file->service->vfscallbacks->seek(file->service, handle, offset, whence);
		debugf("requested offset: %llu, returned offset: %llu", offset, off);
		return off;
	}
	return -1;
}

static int device_vfsclose (void *cookie, void *handle)
{
	upnp_file_t *file;
	debugf("device vfs close");
	file = (upnp_file_t *) handle;
	if (file == NULL) {
		return -1;
	}
	/* is fake file */
	if (file->virtual == 1) {
		free(file->buf);
		free(file);
		return 0;
	}
	/* check services */
	if (file->service != NULL &&
	    file->service->vfscallbacks != NULL &&
	    file->service->vfscallbacks->close != NULL) {
		debugf("calling service close function");
		return file->service->vfscallbacks->close(file->service, handle);
	}
	return 0;
}

static gena_callback_vfs_t device_vfscallbacks = {
	device_vfsgetinfo,
	device_vfsopen,
	device_vfsread,
	device_vfswrite,
	device_vfsseek,
	device_vfsclose,
};

static void device_event_action_request (device_t *device, upnp_event_action_t *event)
{
	int rc;
	device_service_t *service;
	service_action_t *action;

	debugf("received action request:\n"
	       "  errcode     : %d\n"
	       "  actionname  : %s\n"
	       "  udn         : %s\n"
	       "  serviceid   : %s\n",
	       event->errcode,
	       event->action,
	       event->udn,
	       event->serviceid);

	if (strcmp(event->udn, device->uuid) != 0) {
		debugf("discarding event - udn '%s' not recognized", event->udn);
		event->errcode = UPNP_ERROR_INVALID_ACTION;
		return;
	}
	service = device_service_find(device, event->serviceid);
	if (service == NULL) {
		debugf("discarding event - serviceid '%s' not recognized", event->serviceid);
		event->errcode = UPNP_ERROR_INVALID_ACTION;
		return;
	}
	thread_mutex_lock(service->mutex);
	action = service_action_find(service, event->action);
	if (action == NULL) {
		debugf("unknown action '%s' for service '%s'", event->action, event->serviceid);
		event->errcode = UPNP_ERROR_INVALID_ACTION;
		thread_mutex_unlock(service->mutex);
		return;
	}
	if (action->function != NULL) {
		event->errcode = UPNP_ERROR_ACTION_FAILED;
		rc = action->function(service, event);
		if (rc == 0) {
			event->errcode = 0;
			debugf("successful action '%s'", action->name);
		}
	} else {
		debugf("got valid action '%s', but no handler defined", action->name);
	}
	thread_mutex_unlock(service->mutex);
}

static void device_event_subscription_request (device_t *device, upnp_event_subscribe_t *event)
{
	int i;
	device_service_t *service;
	service_variable_t *variable;

	int variable_count;
	char **variable_names;
	char **variable_values;

	debugf("received subscription request:\n"
	       "  serviceid   : %s\n"
	       "  udn         : %s\n"
	       "  sid         : %s\n",
	       event->serviceid,
	       event->udn,
	       event->sid);

	if (strcmp(event->udn, device->uuid) != 0) {
		debugf("discarding event - udn '%s' not recognized", event->udn);
		return;
	}
	service = device_service_find(device, event->serviceid);
	if (service == NULL) {
		debugf("discarding event - serviceid '%s' not recognized", event->serviceid);
		return;
	}
	thread_mutex_lock(service->mutex);
	variable_count = 0;
	for (i = 0; (variable = service->variables[i]) != NULL; i++) {
		if (variable->sendevent == VARIABLE_SENDEVENT_YES) {
			variable_count++;
		}
	}
	debugf("evented variables count: %d", variable_count);
	variable_names = (char **) malloc(sizeof(char *) * (variable_count + 1));
	if (variable_names == NULL) {
		debugf("malloc(sizeof(char *) * (variable_count + 1)) failed");
		thread_mutex_unlock(service->mutex);
		return;
	}
	variable_values = (char **) malloc(sizeof(char *) * (variable_count + 1));
	if (variable_values == NULL) {
		debugf("malloc(sizeof(char *) * (variable_count + 1)) failed");
		thread_mutex_unlock(service->mutex);
		return;
	}
	memset(variable_names, 0, sizeof(char *) * (variable_count + 1));
	memset(variable_values, 0, sizeof(char *) * (variable_count + 1));
	variable_count = 0;
	for (i = 0; (variable = service->variables[i]) != NULL; i++) {
		if (variable->sendevent == VARIABLE_SENDEVENT_YES) {
			variable_names[variable_count] = variable->name;
			variable_values[variable_count] = xml_escape(variable->value, 0);
			debugf("evented '%s' : '%s'", variable_names[variable_count], variable_values[variable_count]);
			variable_count++;
		}
	}
	upnp_accept_subscription(device->upnp, event->udn, event->serviceid, (const char **) variable_names, (const char **) variable_values, variable_count, event->sid);
	for (i = 0; i < variable_count; i++) {
		free(variable_values[i]);
	}
	free(variable_names);
	free(variable_values);
	thread_mutex_unlock(service->mutex);
}

static int device_event_handler (void *cookie, upnp_event_t *event)
{
	device_t *device;
	device = (device_t *) cookie;
	thread_mutex_lock(device->mutex);
	switch (event->type) {
		case UPNP_EVENT_TYPE_SUBSCRIBE_REQUEST:
			device_event_subscription_request(device, &event->event.subscribe);
			break;
		case UPNP_EVENT_TYPE_ACTION:
			device_event_action_request(device, &event->event.action);
			break;
		default:
			break;
	}
	thread_mutex_unlock(device->mutex);
	return 0;
}

int device_init (device_t *device)
{
	int ret;
	char *tmp;
	uuid_gen_t uuid;
	ret = -1;
	debugf("initializing device '%s'", device->name);
	device->mutex = thread_mutex_init("device->mutex", 0);
	if (device->mutex == NULL) {
		debugf("thread_mutex_init(device->mutex, 0) failed");
		goto out;
	}
	debugf("initializing upnp stack");
	device->upnp = upnp_init(device->interface, 0, &device_vfscallbacks, device);
	if (device->upnp == NULL) {
		debugf("upnp_init('%s') failed", device->interface);
		thread_mutex_destroy(device->mutex);
		goto out;
	}
	device->port = upnp_getport(device->upnp);
	device->ipaddress = upnp_getaddress(device->upnp);
	debugf("enabling internal web server");
	upnp_uuid_generate(&uuid);
	if (device->uuid == NULL) {
		device->uuid = (char *) malloc(sizeof(char) * (strlen("uuid:") + 44 + 1));
		if (device->uuid == NULL) {
			goto out;
		}
		sprintf(device->uuid, "uuid:%s", uuid.uuid);
	} else {
		tmp = device->uuid;
		device->uuid = (char *) malloc(sizeof(char) * (strlen("uuid:") + 44 + 1));
		if (device->uuid == NULL) {
			goto out;
		}
		snprintf(device->uuid, (strlen("uuid:") + 44 + 1), "uuid:%s", tmp);
	}
	device->expiretime = 100;
	debugf("generating device '%s' description", device->name);
	device->description = description_generate_from_device(device);
	if (device->description == NULL) {
		debugf("description_generate_from_device(device) failed");
		goto out;
	}
	if (upnp_register_device(device->upnp, device->description, device_event_handler, device) != 0) {
		debugf("upnp_register_device() failed");
		goto out;
	}
	if (upnp_advertise(device->upnp) != 0) {
		debugf("upnp_advertise() failed");
		goto out;
	}
	debugf("listening for control point connections");
	debugf("started device '%s'\n"
	       "  ipaddress  : %s\n"
	       "  port       : %d\n"
	       "  uuid       : %s\n"
	       "  friendly   : %s\n"
	       "  description: %s",
	       device->name,
	       device->ipaddress,
	       device->port,
	       device->uuid,
	       device->friendlyname,
	       device->description);

	ret = 0;
out:	return ret;
}

int device_uninit (device_t *device)
{
	int i;
	int ret;
	device_service_t *service;
	ret = -1;
	debugf("uninitializing device '%s'", device->name);
	thread_mutex_lock(device->mutex);
	debugf("uninitializing services");
	for (i = 0; device->services && ((service = device->services[i]) != NULL); i++) {
		debugf("uninitializing service '%s'", service->name);
		device->services[i] = NULL;
		service_uninit(service);
	}
	debugf("unregistering device '%s'", device->name);
	upnp_uninit(device->upnp);
	thread_mutex_unlock(device->mutex);
	thread_mutex_destroy(device->mutex);
	free(device->services);
	free(device->description);
	free(device->uuid);
	debugf("uninitialized device '%s'", device->name);
	return 0;
}

int device_service_add (device_t *device, device_service_t *service)
{
	int s;
	int ret;
	device_service_t **services;
	ret = -1;
	debugf("adding service '%s' into device '%s'", service->name, device->name);
	for (s = 0; device->services && device->services[s] != NULL; s++) {
		;
	}
	services = (device_service_t **) malloc(sizeof(device_service_t *) * (s + 2));
	if (services == NULL) {
		goto out;
	}
	memset(services, 0, sizeof(device_service_t *) * (s + 2));
	for (s = 0; device->services && device->services[s] != NULL; s++) {
		services[s] = device->services[s];
	}
	service->device = device;
	services[s] = service;
	free(device->services);
	device->services = services;
	ret = 0;
	debugf("added service '%s' into device '%s'", service->name, device->name);
out:	return ret;
}

device_service_t * device_service_find (device_t *device, char *serviceid)
{
	int s;
	device_service_t *service;
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (strcmp(service->id, serviceid) == 0) {
			return service;
		}
	}
	return NULL;
}
