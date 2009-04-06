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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "common.h"
#include "uuid/uuid.h"

#define UPNP_INTERNAL 1

static device_t *__device;

#if defined(UPNP_INTERNAL)
static upnp_t *upnp;
#endif

static int device_vfsgetinfo (void *cookie, char *path, webserver_fileinfo_t *info)
{
	int s;
	device_t *device;
	device_service_t *service;
	debugf("device vfs get info (%s)", path);
	device = (device_t *) cookie;
	pthread_mutex_lock(&device->mutex);
	/* is fake file */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (strcmp(path, service->scpdurl) == 0) {
			info->size = strlen(service->description);
			info->mtime = 0;
			info->mimetype = strdup("text/xml");
			debugf("found service scpd url (%d)", info->size);
			pthread_mutex_unlock(&device->mutex);
			return 0;
		}
	}
	/* check services */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (service->vfscallbacks != NULL && service->vfscallbacks->info != NULL) {
			if (service->vfscallbacks->info(service, path, info) == 0) {
				debugf("found service file");
				pthread_mutex_unlock(&device->mutex);
				return 0;
			}
		}
	}
	pthread_mutex_unlock(&device->mutex);
	return -1;
}

static void * device_vfsopen (void *cookie, char *path, webserver_filemode_t mode)
{
	int s;
	file_t *file;
	device_t *device;
	device_service_t *service;
	debugf("device vfs open (%s)", path);
	device = (device_t *) cookie;
	pthread_mutex_lock(&device->mutex);
	/* is fake file */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (strcmp(path, service->scpdurl) == 0) {
			file = (file_t *) malloc(sizeof(file_t));
			if (file == NULL) {
				pthread_mutex_unlock(&device->mutex);
				return NULL;
			}
			memset(file, 0, sizeof(file_t));
			file->virtual = 1;
			file->size = strlen(service->description);
			file->buf = strdup(service->description);
			pthread_mutex_unlock(&device->mutex);
			return file;
		}
	}
	/* check services */
	for (s = 0; (service = device->services[s]) != NULL; s++) {
		if (service->vfscallbacks != NULL && service->vfscallbacks->open != NULL) {
			file = service->vfscallbacks->open(service, path, mode);
			if (file != NULL) {
				debugf("found service file");
				pthread_mutex_unlock(&device->mutex);
				return file;
			}
		}
	}
	pthread_mutex_unlock(&device->mutex);
	return NULL;
}

static int device_vfsread (void *cookie, void *handle, char *buffer, unsigned int length)
{
	size_t len;
	file_t *file;
	debugf("device vfs read");
	file = (file_t *) handle;
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

static unsigned long device_vfsseek (void *cookie, void *handle, long offset, webserver_seek_t whence)
{
	file_t *file;
	unsigned long off;
	debugf("device vfs seek");
	file = (file_t *) handle;
	if (file == NULL) {
		return -1;
	}
	/* is fake file */
	if (file->virtual == 1) {
		switch (whence) {
			case WEBSERVER_SEEK_SET: file->offset = offset; break;
			case WEBSERVER_SEEK_CUR: file->offset += offset; break;
			case WEBSERVER_SEEK_END: file->offset = file->size + offset; break;
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
		debugf("requested offset: %lu, returned offset: %lu", offset, off);
		return off;
	}
	return -1;
}

static int device_vfsclose (void *cookie, void *handle)
{
	file_t *file;
	debugf("device vfs close");
	file = (file_t *) handle;
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

static webserver_callbacks_t device_vfscallbacks = {
	device_vfsgetinfo,
	device_vfsopen,
	device_vfsread,
	device_vfswrite,
	device_vfsseek,
	device_vfsclose,
};

static int webserver_get_info (const char *filename, struct File_Info *info)
{
	int i;
	icon_t *icon;
	device_t *device;
	debugf("filename: %s", filename);
	if (strncmp("/upnpd/", filename, 7) != 0) {
		return -1;
	}
	device = (device_t *) __device;
	for (i = 0; (icon = device->icons[i]) != NULL; i++) {
		if (strcmp(filename, icon->url) == 0) {
			debugf("found file");
			info->file_length = icon->size;
			info->last_modified = 0;
			info->is_directory = 0;
			info->is_readable = 1;
			info->content_type = strdup(icon->mimetype);
			return 0;
		}
	}
	debugf("could not find file");
	return -1;
}

static UpnpWebFileHandle webserver_open (const char *filename, enum UpnpOpenFileMode mode)
{
	int i;
	file_t *file;
	icon_t *icon;
	device_t *device;
	debugf("filename: %s", filename);
	if (strncmp("/upnpd/", filename, 7) != 0) {
		return NULL;
	}
	device = (device_t *) __device;
	for (i = 0; (icon = device->icons[i]) != NULL; i++) {
		if (strcmp(filename, icon->url) == 0) {
			debugf("found file");
			file = (file_t *) malloc(sizeof(file_t));
			if (file == NULL) {
				return NULL;
			}
			memset(file, 0, sizeof(file_t));
			file->buf = (char *) icon->buffer;
			file->fd = 0;
			file->offset = 0;
			file->service = NULL;
			file->size = icon->size;
			file->virtual = 1;
			return file;
		}
	}
	debugf("could not find file");
	return NULL;
}

static int webserver_read (UpnpWebFileHandle fh, char *buf, size_t buflen)
{
	int len;
	file_t *file;
	file = (file_t *) fh;
	len = (buflen < file->size - file->offset) ? buflen : file->size - file->offset;
	if (len >= 0) {
		memcpy(buf, file->buf + file->offset, len);
		file->offset += len;
	}
	return len;
}

static int webserver_write (UpnpWebFileHandle fh, char *buf, size_t buflen)
{
	return -1;
}

static int webserver_seek (UpnpWebFileHandle fh, off_t offset, int origin)
{
	int pos;
	file_t *file;
	file = (file_t *) fh;
	pos = -1;
	switch (origin) {
		case SEEK_SET:
			pos = offset;
			break;
		case SEEK_CUR:
			pos = file->offset + offset;
			break;
		case SEEK_END:
			pos = file->size + offset;
			break;
	}
	if (pos < 0 || pos > file->size) {
		return -1;
	}
	file->offset = pos;
	return 0;
}

static int webserver_close (UpnpWebFileHandle fh)
{
	file_t *file;
	file = (file_t *) fh;
	free(file);
	return 0;
}

struct UpnpVirtualDirCallbacks virtual_dir_callbacks = {
	webserver_get_info,
	webserver_open,
	webserver_read,
	webserver_write,
	webserver_seek,
	webserver_close
};

#if defined(UPNP_INTERNAL)
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
		event->errcode = UPNP_SOAP_E_INVALID_ACTION;
		return;
	}
	service = device_service_find(device, event->serviceid);
	if (service == NULL) {
		debugf("discarding event - serviceid '%s' not recognized", event->serviceid);
		event->errcode = UPNP_SOAP_E_INVALID_ACTION;
		return;
	}
	pthread_mutex_lock(&service->mutex);
	action = service_action_find(service, event->action);
	if (action == NULL) {
		debugf("unknown action '%s' for service '%s'", event->action, event->serviceid);
		event->errcode = UPNP_SOAP_E_INVALID_ACTION;
		pthread_mutex_unlock(&service->mutex);
		return;
	}
	if (action->function != NULL) {
		rc = action->function(service, event);
		if (rc == 0) {
			event->errcode = UPNP_E_SUCCESS;
			debugf("successful action '%s'", action->name);
		}
		if (event->response == NULL) {
			event->response = UpnpMakeActionResponse(event->action, event->serviceid, 0, NULL);
		}
	} else {
		debugf("got valid action '%s', but no handler defined", action->name);
	}
	pthread_mutex_unlock(&service->mutex);
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
	pthread_mutex_lock(&service->mutex);
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
		pthread_mutex_unlock(&service->mutex);
		return;
	}
	variable_values = (char **) malloc(sizeof(char *) * (variable_count + 1));
	if (variable_values == NULL) {
		debugf("malloc(sizeof(char *) * (variable_count + 1)) failed");
		pthread_mutex_unlock(&service->mutex);
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
	upnp_accept_subscription(upnp, event->udn, event->serviceid, (const char **) variable_names, (const char **) variable_values, variable_count, event->sid);
	for (i = 0; i < variable_count; i++) {
		free(variable_values[i]);
	}
	free(variable_names);
	free(variable_values);
	pthread_mutex_unlock(&service->mutex);
}

static int device_event_handler (void *cookie, upnp_event_t *event)
{
	device_t *device;
	device = (device_t *) cookie;
	pthread_mutex_lock(&device->mutex);
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
	pthread_mutex_unlock(&device->mutex);
	return 0;
}
#else
static int device_event_get_var_request (device_t *device, struct Upnp_State_Var_Request *event)
{
	debugf("received get var request:\n"
	       "  errcode     : %d\n"
	       "  errstr      : %s\n"
	       "  udn         : %s\n"
	       "  serviceid   : %s\n"
	       "  statevarname: %s\n"
	       "  currentval  : %s\n",
	       event->ErrCode,
	       event->ErrStr,
	       event->DevUDN,
	       event->ServiceID,
	       event->StateVarName,
	       event->CurrentVal);
	assert(0);
	return 0;
}

static void device_event_action_request (device_t *device, struct Upnp_Action_Request *event)
{
	int rc;
	device_service_t *service;
	service_action_t *action;

	debugf("received action request:\n"
	       "  errcode     : %d\n"
	       "  errstr      : %s\n"
	       "  actionname  : %s\n"
	       "  udn         : %s\n"
	       "  serviceid   : %s\n",
	       event->ErrCode,
	       event->ErrStr,
	       event->ActionName,
	       event->DevUDN,
	       event->ServiceID);

	if (strcmp(event->DevUDN, device->uuid) != 0) {
		debugf("discarding event - udn '%s' not recognized", event->DevUDN);
		event->ActionResult = NULL;
		event->ErrCode = UPNP_SOAP_E_INVALID_ACTION;
		strcpy(event->ErrStr, "unrecognized device");
		return;
	}
	service = device_service_find(device, event->ServiceID);
	if (service == NULL) {
		debugf("discarding event - serviceid '%s' not recognized", event->ServiceID);
		return;
	}
	pthread_mutex_lock(&service->mutex);
	action = service_action_find(service, event->ActionName);
	if (action == NULL) {
		debugf("unknown action '%s' for service '%s'", event->ActionName, event->ServiceID);
		event->ActionResult = NULL;
		event->ErrCode = UPNP_SOAP_E_INVALID_ACTION;
		strcpy(event->ErrStr, "invalid action");
		pthread_mutex_unlock(&service->mutex);
		return;
	}
	if (action->function != NULL) {
		rc = action->function(service, event);
		if (rc == 0) {
			event->ErrCode = UPNP_E_SUCCESS;
			debugf("successfull action '%s'", action->name);
		}
		if (event->ActionResult == NULL) {
			event->ActionResult = UpnpMakeActionResponse(event->ActionName, event->ServiceID, 0, NULL);
		}
	} else {
		debugf("got valid action '%s', but no handler defined", action->name);
	}
	pthread_mutex_unlock(&service->mutex);
}

static void device_event_subscription_request (device_t *device, struct Upnp_Subscription_Request *event)
{
	int i;
	int rc;
	device_service_t *service;
	service_variable_t *variable;

	int variable_count;
	char **variable_names;
	char **variable_values;

	debugf("received subscription request:\n"
	       "  serviceid   : %s\n"
	       "  udn         : %s\n"
	       "  sid         : %s\n",
	       event->ServiceId,
	       event->UDN,
	       event->Sid);

	if (strcmp(event->UDN, device->uuid) != 0) {
		debugf("discarding event - udn '%s' not recognized", event->UDN);
		return;
	}
	service = device_service_find(device, event->ServiceId);
	if (service == NULL) {
		debugf("discarding event - serviceid '%s' not recognized", event->ServiceId);
		return;
	}
	pthread_mutex_lock(&service->mutex);
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
		pthread_mutex_unlock(&service->mutex);
		return;
	}
	variable_values = (char **) malloc(sizeof(char *) * (variable_count + 1));
	if (variable_values == NULL) {
		debugf("malloc(sizeof(char *) * (variable_count + 1)) failed");
		pthread_mutex_unlock(&service->mutex);
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
	rc = UpnpAcceptSubscription(device->handle, event->UDN, event->ServiceId, (const char **) variable_names, (const char **) variable_values, variable_count, event->Sid);
	if (rc != UPNP_E_SUCCESS) {
		debugf("UpnpAcceptSubscription() failed (%d:%s)", rc, UpnpGetErrorMessage(rc));
	}
	for (i = 0; i < variable_count; i++) {
		free(variable_values[i]);
	}
	free(variable_names);
	free(variable_values);
	pthread_mutex_unlock(&service->mutex);
}

static int device_event_handler (Upnp_EventType eventtype, void *event, void *cookie)
{
	device_t *device;
	device = (device_t *) cookie;
	pthread_mutex_lock(&device->mutex);
	switch (eventtype) {
		case UPNP_CONTROL_ACTION_REQUEST:
			device_event_action_request(device, (struct Upnp_Action_Request *) event);
			break;
		case UPNP_CONTROL_GET_VAR_REQUEST:
			device_event_get_var_request(device, (struct Upnp_State_Var_Request *) event);
			break;
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
			device_event_subscription_request(device, (struct Upnp_Subscription_Request *) event);
			break;
	        /* ignore below */
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	        case UPNP_DISCOVERY_SEARCH_RESULT:
	        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
	        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	        case UPNP_CONTROL_ACTION_COMPLETE:
	        case UPNP_CONTROL_GET_VAR_COMPLETE:
	        case UPNP_EVENT_RECEIVED:
	        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
	        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
	        case UPNP_EVENT_RENEWAL_COMPLETE:
	        case UPNP_EVENT_AUTORENEWAL_FAILED:
	        case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
	        	assert(0);
	        	break;
	}
	pthread_mutex_unlock(&device->mutex);
	return 0;
}
#endif

int device_init (device_t *device)
{
	int ret;
	uuid_t uuid;
#if defined(UPNP_INTERNAL)
#else
	int rc;
#endif
	ret = -1;
	debugf("initializing device '%s'", device->name);
	if (pthread_mutex_init(&device->mutex, NULL) != 0) {
		debugf("pthread_mutex_init(&device->mutex, NULL) failed");
		goto out;
	}
	debugf("initializing upnp stack");
#if defined(UPNP_INTERNAL)
	upnp = upnp_init(device->interface, 0);
	if (upnp == NULL) {
		pthread_mutex_destroy(&device->mutex);
		goto out;
	}
	device->port = upnp_getport(upnp);
	device->ipaddress = upnp_getaddress(upnp);
#else
	if ((rc = UpnpInit(device->interface, 0)) != UPNP_E_SUCCESS) {
		debugf("UpnpInit(NULL, 0) failed (%d:%s)", rc, UpnpGetErrorMessage(rc));
		pthread_mutex_destroy(&device->mutex);
		UpnpFinish();
		goto out;
	}
	device->port = UpnpGetServerPort();
	device->ipaddress = UpnpGetServerIpAddress();
	device_vfscallbacks.cookie = device;
#endif
	debugf("enabling internal web server");
	device->webserver = webserver_init(device->ipaddress, device->port, &device_vfscallbacks);
	uuid_generate(uuid);
	device->uuid = (char *) malloc(sizeof(char) * (strlen("uuid:") + 44 + 1));
	if (device->uuid == NULL) {
		goto out;
	}
	sprintf(device->uuid, "uuid:");
	uuid_unparse_lower(uuid, device->uuid + strlen(device->uuid));
	device->expiretime = 100;
	debugf("generating device '%s' description", device->name);
	device->description = description_generate_from_device(device);
	if (device->description == NULL) {
		debugf("description_generate_from_device(device) failed");
		goto out;
	}
#if defined(UPNP_INTERNAL)
	if (upnp_register_device(upnp, device->description, device_event_handler, device) != 0) {
		goto out;
	}
	if (upnp_advertise(upnp) != 0) {
		goto out;
	}
#else
	debugf("enabling upnp webserver");
	if ((rc = UpnpEnableWebserver(TRUE)) != UPNP_E_SUCCESS) {
		debugf("UpnpenableWebserver() failed");
		goto out;
	}
	__device = device;
	if ((rc = UpnpSetVirtualDirCallbacks(&virtual_dir_callbacks)) != UPNP_E_SUCCESS) {
		debugf("UpnpSetVirtualDirCallbacks() failed");
		goto out;
	}
	if ((rc = UpnpAddVirtualDir("/upnpd")) != UPNP_E_SUCCESS) {
		debugf("UpnpAddVirtualDir() failed");
		goto out;
	}
	debugf("registering root device '%s'", device->name);
	if ((rc = UpnpRegisterRootDevice2(UPNPREG_BUF_DESC, device->description, strlen(device->description), 1, device_event_handler, device, &device->handle)) != UPNP_E_SUCCESS) {
		debugf("UpnpRegisterRootDevice2() failed (%d:%s)", rc, UpnpGetErrorMessage(rc));
		goto out;
	}
	debugf("sending advertisement for device '%s' (expiretime:%d seconds)", device->name, device->expiretime);
	if ((rc = UpnpSendAdvertisement(device->handle, device->expiretime)) != UPNP_E_SUCCESS) {
		debugf("UpnpSendAdvertisement() failed (%d:%s)", rc, UpnpGetErrorMessage(rc));
		goto out;
	}
#endif
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
	debugf("uninitializing webserver");
	webserver_uninit(device->webserver);
	debugf("uninitializing device '%s'", device->name);
	pthread_mutex_lock(&device->mutex);
	debugf("uninitializing services");
	for (i = 0; device->services && ((service = device->services[i]) != NULL); i++) {
		debugf("uninitializing service '%s'", service->name);
		device->services[i] = NULL;
		service_uninit(service);
	}
	debugf("unregistering device '%s'", device->name);
#if defined(UPNP_INTERNAL)
	upnp_uninit(upnp);
#else
	UpnpUnRegisterRootDevice(device->handle);
	UpnpFinish();
#endif
	pthread_mutex_unlock(&device->mutex);
	pthread_mutex_destroy(&device->mutex);
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
