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
#include <unistd.h>
#include <inttypes.h>

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "uuid.h"
#include "common.h"

static upnp_t *upnp;

static int client_variable_uninit (client_variable_t *variable)
{
	free(variable->name);
	free(variable->value);
	free(variable);
	return 0;
}

static int client_service_uninit (client_service_t *service)
{
	client_variable_t *variable;
	client_variable_t *variablen;
	list_for_each_entry_safe(variable, variablen, &service->variables, head) {
		list_del(&variable->head);
		client_variable_uninit(variable);
	}
	free(service->type);
	free(service->id);
	free(service->sid);
	free(service->controlurl);
	free(service->eventurl);
	free(service);
	return 0;
}

static int client_device_uninit (client_device_t *device)
{
	client_service_t *service;
	client_service_t *servicen;
	list_for_each_entry_safe(service, servicen, &device->services, head) {
		list_del(&service->head);
		client_service_uninit(service);
	}
	free(device->name);
	free(device->type);
	free(device->uuid);
	free(device);
	return 0;
}

static client_device_t * client_device_init (char *type, char *uuid, char *friendlyname, int expiretime)
{
	client_device_t *device;
	if (type == NULL ||
	    uuid == NULL ||
	    friendlyname == NULL) {
		return NULL;
	}
	device = (client_device_t *) malloc(sizeof(client_device_t));
	if (device == NULL) {
		return NULL;
	}
	memset(device, 0, sizeof(client_device_t));
	device->type = strdup(type);
	device->uuid = strdup(uuid);
	device->name = strdup(friendlyname);
	device->expiretime = expiretime;
	if (device->type == NULL ||
	    device->uuid == NULL ||
	    device->name == NULL) {
		client_device_uninit(device);
		return NULL;
	}
	list_init(&device->services);
	return device;
}

static int client_service_subscribe (client_t *client, client_service_t *service)
{
	int t;
	int rc;
	debugf("subscribing to '%s'", service->eventurl);
	t = 1801;
	rc = upnp_subscribe(upnp, service->eventurl, &t, &service->sid);
	if (rc != 0) {
		debugf("upnp_subscribe() failed");
		return -1;
	}
	return 0;
}

typedef struct client_parser_service_s {
	char *serviceId;
	char *serviceType;
	char *SCPDURL;
	char *controlURL;
	char *eventSubURL;
} client_parser_service_t;

typedef struct client_parser_device_s {
	char *deviceType;
	char *UDN;
	char *friendlyName;
	int nservices;
	client_parser_service_t *service;
	client_parser_service_t *services;
} client_parser_device_t;

typedef struct client_parser_data_s {
	int ndevices;
	client_parser_device_t *device;
	client_parser_device_t *devices;
	char *URLBase;
} client_parser_data_t;

static int client_parser_callback (void *context, const char *path, const char *name, const char **atrr, const char *value)
{
	client_parser_data_t *data;
	client_parser_device_t *devices;
	client_parser_service_t *services;
	data = (client_parser_data_t *) context;
	if (strcmp(path, "/root/URLBase") == 0) {
		data->URLBase = strdup(value);
	} else if (strcmp(path, "/root/device") == 0) {
		devices = (client_parser_device_t *) malloc(sizeof(client_parser_device_t) * (data->ndevices + 1));
		if (devices == NULL) {
			data->device = NULL;
			return 0;
		}
		memset(devices, 0, sizeof(client_parser_device_t) * (data->ndevices + 1));
		if (data->ndevices > 0) {
			memcpy(devices, data->devices, sizeof(client_parser_device_t) * data->ndevices);
		}
		data->device = &devices[data->ndevices];
		free(data->devices);
		data->devices = devices;
		data->ndevices += 1;
	} else if (data->device != NULL) {
		if (strcmp(path, "/root/device/deviceType") == 0) {
			data->device->deviceType = strdup(value);
		} else if (strcmp(path, "/root/device/UDN") == 0) {
			data->device->UDN = strdup(value);
		} else if (strcmp(path, "/root/device/friendlyName") == 0) {
			data->device->friendlyName = strdup(value);
		} else if (strcmp(path, "/root/device/serviceList/service") == 0) {
			services = (client_parser_service_t *) malloc(sizeof(client_parser_service_t) * (data->device->nservices + 1));
			if (services == NULL) {
				data->device->service = NULL;
				return 0;
			}
			memset(services, 0, sizeof(client_parser_service_t) * (data->device->nservices + 1));
			if (data->device->nservices > 0) {
				memcpy(services, data->device->services, sizeof(client_parser_service_t) * data->device->nservices);
			}
			data->device->service = &services[data->device->nservices];
			free(data->device->services);
			data->device->services = services;
			data->device->nservices += 1;
		} else if (data->device->service != NULL) {
			if (strcmp(path, "/root/device/serviceList/service/serviceId") == 0) {
				data->device->service->serviceId = strdup(value);
			} else if (strcmp(path, "/root/device/serviceList/service/serviceType") == 0) {
				data->device->service->serviceType = strdup(value);
			} else if (strcmp(path, "/root/device/serviceList/service/SCPDURL") == 0) {
				data->device->service->SCPDURL = strdup(value);
			} else if (strcmp(path, "/root/device/serviceList/service/controlURL") == 0) {
				data->device->service->controlURL = strdup(value);
			} else if (strcmp(path, "/root/device/serviceList/service/eventSubURL") == 0) {
				data->device->service->eventSubURL = strdup(value);
			}
		}
	}
	return 0;
}

static client_service_t * client_service_init (client_parser_device_t *device, char *baseURL, char *location, char *servicetype)
{
	int s;
	int ret;
	char *base;
	char *relcontrolURL = NULL;
	char *releventURL = NULL;

	client_service_t *service = NULL;

	if (baseURL) {
		base = baseURL;
	} else {
		base = location;
	}
	for (s = 0; s < device->nservices; s++) {
		if (strcmp(device->services[s].serviceType, servicetype) == 0) {
			debugf("found service: %s", servicetype);
			service = (client_service_t *) malloc(sizeof(client_service_t));
			if (service == NULL) {
				break;
			}
			memset(service, 0, sizeof(client_service_t));
			list_init(&service->variables);
			service->type = strdup(servicetype);
			service->id = strdup(device->services[s].serviceId);
			relcontrolURL = device->services[s].controlURL;
			releventURL = device->services[s].eventSubURL;
			service->controlurl = malloc(strlen(base) + strlen(relcontrolURL) + 1);
			if (service->controlurl) {
				ret = upnp_resolveurl(base, relcontrolURL, service->controlurl);
				if (ret != 0) {
					debugf("error generating control url from '%s' + '%s'", base, relcontrolURL);
					free(service->controlurl);
					service->controlurl = NULL;
				}
			}
			service->eventurl = malloc(strlen(base) + strlen(releventURL) + 1);
			if (service->eventurl) {
				ret = upnp_resolveurl(base, releventURL, service->eventurl);
				if (ret != 0) {
					debugf("error generating event url from '%s' + '%s'", base, releventURL);
					free(service->eventurl);
					service->eventurl = NULL;
				}
			}
			if (service->type == NULL ||
			    service->id == NULL ||
			    service->controlurl == NULL ||
			    service->eventurl == NULL) {
				client_service_uninit(service);
				service = NULL;
			} else {
				debugf("adding new service:");
				debugf("  type      :'%s'", service->type);
				debugf("  id        :'%s'", service->id);
				debugf("  controlurl:'%s'", service->controlurl);
				debugf("  eventurl  :'%s'", service->eventurl);
			}
			break;
		}
	}
	return service;
}

static int client_event_advertisement_alive (client_t *client, upnp_event_advertisement_t *advertisement)
{
	int s;
	int d;
	char *buffer;
	client_device_t *device;
	client_service_t *service;
	device_description_t *description;

	client_parser_data_t data;

	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		if (strcmp(advertisement->device, description->device) == 0) {
			goto found;
		}
	}
	return 0;
found:
	list_for_each_entry(device, &client->devices, head) {
		if (strcmp(advertisement->uuid, device->uuid) == 0) {
			device->expiretime = advertisement->expires;
			return 0;
		}
	}
	debugf("downloading device description from '%s'", advertisement->location);
	memset(&data, 0, sizeof(client_parser_data_t));
	buffer = upnp_download(upnp, advertisement->location);
	if (buffer == NULL) {
		debugf("upnp_download('%s') failed", advertisement->location);
		goto out;
	}
	if (xml_parse_buffer_callback(buffer, strlen(buffer), client_parser_callback, &data) != 0) {
		debugf("xml_parse_buffer_callback() failed");
		goto out;
	}
	for (d = 0; d < data.ndevices; d++) {
		if (data.devices[d].UDN != NULL &&
			data.devices[d].deviceType != NULL &&
			data.devices[d].friendlyName != NULL) {
			debugf("new device:\n"
				   "  UDN         : '%s'\n"
				   "  devceType   : '%s'\n"
				   "  friendlyName: '%s'\n",
				   data.devices[d].UDN,
				   data.devices[d].deviceType,
				   data.devices[d].friendlyName);
			device = client_device_init(data.devices[d].deviceType, data.devices[d].UDN, data.devices[d].friendlyName, advertisement->expires);
			if (device != NULL) {
				for (s = 0; description->services[s] != NULL; s++) {
					service = client_service_init(&data.devices[d], data.URLBase, advertisement->location, description->services[s]);
					if (service != NULL) {
						list_add(&service->head, &device->services);
						if (client_service_subscribe(client, service) != 0) {
							debugf("client_service_subscribe(service); failed");
							client_device_uninit(device);
							goto out;
						}
					}
				}
			}
		}
	}
	list_add_tail(&device->head, &client->devices);
	debugf("added '%s' to device list", device->uuid);
out:	free(buffer);
	for (d = 0; d < data.ndevices; d++) {
		for (s = 0; s < data.devices[d].nservices; s++) {
			free(data.devices[d].services[s].SCPDURL);
			free(data.devices[d].services[s].controlURL);
			free(data.devices[d].services[s].eventSubURL);
			free(data.devices[d].services[s].serviceId);
			free(data.devices[d].services[s].serviceType);
		}
		free(data.device[d].UDN);
		free(data.device[d].deviceType);
		free(data.device[d].friendlyName);
		free(data.devices[d].services);
	}
	free(data.devices);
	free(data.URLBase);
	return 0;
}

static int client_event_advertisement_byebye (client_t *client, upnp_event_advertisement_t *advertisement)
{
	int d;
	client_device_t *device;
	client_device_t *devicen;
	device_description_t *description;
	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		if (strcmp(advertisement->device, description->device) == 0) {
			goto found;
		}
	}
	return 0;
found:
	list_for_each_entry_safe(device, devicen, &client->devices, head) {
		if (strcmp(advertisement->uuid, device->uuid) == 0) {
			list_del(&device->head);
			debugf("removed '%s' from device list", device->uuid);
			client_device_uninit(device);
		}
	}
	return 0;
}

static int client_event_handler (void *cookie, upnp_event_t *event)
{
	client_t *client;
	client = (client_t *) cookie;
	thread_mutex_lock(client->mutex);
	switch (event->type) {
		case UPNP_EVENT_TYPE_ADVERTISEMENT_ALIVE:
			client_event_advertisement_alive(client, &event->event.advertisement);
			break;
		case UPNP_EVENT_TYPE_ADVERTISEMENT_BYEBYE:
			client_event_advertisement_byebye(client, &event->event.advertisement);
			break;
		case UPNP_EVENT_TYPE_SUBSCRIBE_REQUEST:
		case UPNP_EVENT_TYPE_ACTION:
		case UPNP_EVENT_TYPE_UNKNOWN:
			break;
	}
	thread_mutex_unlock(client->mutex);
	return 0;
}

static void * client_timer (void *arg)
{
	int stamp;
	client_t *client;
	client_device_t *device;
	client_device_t *devicen;

	client = (client_t *) arg;
	stamp = 30;

	thread_mutex_lock(client->mutex);
	debugf("started timer thread");
	client->timer_running = 1;
	thread_cond_broadcast(client->cond);
	thread_mutex_unlock(client->mutex);

	while (1) {
		thread_mutex_lock(client->mutex);
		thread_cond_timedwait(client->cond, client->mutex, stamp * 1000);
		if (client->running == 0) {
			goto out;
		}
		debugf("checking for expire times");
		list_for_each_entry_safe(device, devicen, &client->devices, head) {
			device->expiretime -= stamp;
			if (device->expiretime < 0) {
				list_del(&device->head);
				debugf("removed '%s' from device list", device->uuid);
				client_device_uninit(device);
			} else if (device->expiretime < stamp) {
#if 1
				debugf("sending search request for '%s'", "upnp:rootdevice");
				if (upnp_search(upnp, 2, "upnp:rootdevice") != 0) {
					debugf("error sending search request for %s", "upnp:rootdevice");
				}
#else
				debugf("sending search request for '%s'", device->uuid);
				if (upnp_search(upnp, 2, device->uuid) != 0) {
					debugf("error sending search request for %s", device->uuid);
				}
#endif
			}
		}
		thread_mutex_unlock(client->mutex);
	}
out:	client->timer_running = 0;
	debugf("stopped timer thread");
	thread_cond_broadcast(client->cond);
	thread_mutex_unlock(client->mutex);

	return NULL;
}

int client_init (client_t *client)
{
	int ret;
	ret = -1;
	debugf("initializing client '%s'", client->name);
	client->mutex = thread_mutex_init("client->mutex", 0);
	if (client->mutex == NULL) {
		debugf("thread_mutex_init(client->mutex, 0) failed");
		goto out;
	}
	client->cond = thread_cond_init("client->cond");
	if (client->cond == NULL) {
		debugf("thread_cond_init(client->cond) failed");
		thread_mutex_destroy(client->mutex);
		goto out;
	}
	debugf("initializing devices list");
	list_init(&client->devices);
	debugf("initializing upnp stack");
	upnp = upnp_init(client->interface, client->ifmask, 0, NULL, NULL);
	if (upnp == NULL) {
		debugf("upnp_init() failed");
		thread_cond_destroy(client->cond);
		thread_mutex_destroy(client->mutex);
		goto out;
	}
	client->port = upnp_getport(upnp);
	client->ipaddress = upnp_getaddress(upnp);
	debugf("registering client device '%s'", client->name);
	if (upnp_register_client(upnp, client_event_handler, client)) {
		upnp_uninit(upnp);
		thread_cond_destroy(client->cond);
		thread_mutex_destroy(client->mutex);
		goto out;
	}
	client->running = 1;
	debugf("started client '%s'\n"
	       "  ipaddress  : %s\n"
	       "  port       : %d",
	       client->name,
	       client->ipaddress,
	       client->port);

	thread_mutex_lock(client->mutex);
	debugf("starting timer thread");
	client->timer_thread = thread_create("client_timer", client_timer, client);
	while (client->timer_running == 0) {
		thread_cond_wait(client->cond, client->mutex);
	}
	debugf("timer is up and running");
	thread_mutex_unlock(client->mutex);

	client_refresh(client, 1);
	ret = 0;
out:	return ret;
}

int client_uninit (client_t *client)
{
	int ret;
	client_device_t *device;
	client_device_t *devicen;
	ret = -1;
	debugf("uninitializing client '%s'", client->name);
	upnp_uninit(upnp);
	thread_mutex_lock(client->mutex);
	client->running = 0;
	thread_cond_broadcast(client->cond);
	thread_mutex_unlock(client->mutex);
	debugf("waiting for timer thread to finish");
	thread_mutex_lock(client->mutex);
	while (client->timer_running != 0) {
		thread_cond_wait(client->cond, client->mutex);
	}
	debugf("joining timer thread");
	thread_join(client->timer_thread);
	list_for_each_entry_safe(device, devicen, &client->devices, head) {
		list_del(&device->head);
		client_device_uninit(device);
	}
	debugf("unregistering client '%s'", client->name);
	thread_mutex_unlock(client->mutex);
	thread_cond_destroy(client->cond);
	thread_mutex_destroy(client->mutex);
	debugf("uninitialized client '%s'", client->name);
	return 0;
}

int client_refresh (client_t *client, int remove)
{
	int ret;
	client_device_t *device;
	client_device_t *devicen;
	ret = 0;
	thread_mutex_lock(client->mutex);
	debugf("refreshing device list");
	if (remove != 0) {
		debugf("cleaning device list");
		list_for_each_entry_safe(device, devicen, &client->devices, head) {
			list_del(&device->head);
			client_device_uninit(device);
		}
	}
	debugf("sending requests");
#if 0
	int d;
	device_description_t *description;
	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		if (upnp_search(upnp, 2, description->device) != 0) {
			debugf("error sending search request for %s", description->device);
		}
	}
#else
	if (upnp_search(upnp, 2, "upnp:rootdevice") != 0) {
		debugf("error sending search request for %s", "upnp:rootdevice");
	}
#endif
	thread_mutex_unlock(client->mutex);
	return ret;
}

char * client_action (client_t *client, const char *devicename, const char *servicetype, const char *actionname, char **param_name, char **param_val, int param_count)
{
	int rc;
	char *response;
	client_device_t *device;
	client_service_t *service;

	rc = 0;
	response = NULL;

	thread_mutex_lock(client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		if (strcmp(device->name, devicename) == 0) {
			goto found_device;
		}
	}
	debugf("could not find device");
	thread_mutex_unlock(client->mutex);
	return NULL;

found_device:
	list_for_each_entry(service, &device->services, head) {
		if (strcmp(service->type, servicetype) == 0) {
			goto found_service;
		}
	}
	debugf("could not find service");
	thread_mutex_unlock(client->mutex);
	return NULL;

found_service:
	debugf("generating action");
	response = upnp_makeaction(upnp, actionname, service->controlurl, service->type, param_count, param_name, param_val);
	if (response == NULL) {
		debugf("upnp_makeaction() failed");
		thread_mutex_unlock(client->mutex);
		return NULL;
	}
	thread_mutex_unlock(client->mutex);

	return response;
}
