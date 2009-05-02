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
#include <inttypes.h>
#include <pthread.h>

#include "upnp.h"
#include "upnpd.h"
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

static client_service_t * client_service_init (IXML_Document *desc, char *location, char *servicetype)
{
	int i;
	int length;
	int ret;
	char *tempServiceType = NULL;
	char *baseURL = NULL;
	char *base;
	char *relcontrolURL = NULL;
	char *releventURL = NULL;
	IXML_NodeList *serviceList = NULL;
	IXML_Element *servicedesc = NULL;

	client_service_t *service = NULL;

	baseURL = xml_get_first_document_item(desc, "URLBase" );
	if (baseURL) {
		base = baseURL;
	} else {
		base = location;
	}
	serviceList = xml_get_first_service_list(desc);
	length = ixmlNodeList_length(serviceList);
	for (i = 0; i < length; i++) {
		servicedesc = (IXML_Element *) ixmlNodeList_item(serviceList, i);
		tempServiceType = xml_get_first_element_item((IXML_Element *) servicedesc, "serviceType");
		if (strcmp(tempServiceType, servicetype) == 0) {
			debugf("found service: %s", servicetype);
			service = (client_service_t *) malloc(sizeof(client_service_t));
			if (service == NULL) {
				break;
			}
			memset(service, 0, sizeof(client_service_t));
			list_init(&service->variables);
			service->type = strdup(servicetype);
			service->id = xml_get_first_element_item(servicedesc, "serviceId");
			relcontrolURL = xml_get_first_element_item(servicedesc, "controlURL");
			releventURL = xml_get_first_element_item(servicedesc, "eventSubURL");
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
			if (relcontrolURL) {
				free(relcontrolURL);
			}
			if (releventURL) {
				free(releventURL);
			}
			relcontrolURL = NULL;
			releventURL = NULL;

			if (service->type == NULL ||
			    service->id == NULL ||
			    service->controlurl == NULL ||
			    service->eventurl == NULL) {
				client_service_uninit(service);
				service = NULL;
			}
			break;
		}
		if (tempServiceType) {
			free(tempServiceType);
		}
		tempServiceType = NULL;
	}
	if (tempServiceType) {
		free(tempServiceType);
	}
	if (serviceList) {
		ixmlNodeList_free(serviceList);
	}
	if (baseURL) {
		free(baseURL);
	}
	return service;
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

static int client_event_advertisement_alive (client_t *client, upnp_event_advertisement_t *advertisement)
{
	int d;
	int s;
	char *buffer;
	client_device_t *device;
	client_service_t *service;
	device_description_t *description;

	char *uuid = NULL;
	char *devicetype = NULL;
	char *friendlyname = NULL;
	IXML_Document *desc = NULL;

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
	buffer = upnp_download(upnp, advertisement->location);
	if (buffer == NULL) {
		debugf("upnp_download('%s') failed", advertisement->location);
		return 0;
	}
	if (ixmlParseBufferEx(buffer, &desc) != IXML_SUCCESS) {
		debugf("ixmlParseBufferEx() failed");
		free(buffer);
		return 0;
	}
	debugf("reading elements from document '%s'", advertisement->location);
	uuid = xml_get_first_document_item(desc, "UDN");
	devicetype = xml_get_first_document_item(desc, "deviceType");
	friendlyname = xml_get_first_document_item(desc, "friendlyName");
	debugf("elements:\n"
		"  uuid        : %s\n"
		"  devicetype  : %s\n"
		"  friendlyname: %s\n",
		uuid,
		devicetype,
		friendlyname);
	device = client_device_init(devicetype, uuid, friendlyname, advertisement->expires);
	if (device == NULL) {
		goto out;
	}
	for (s = 0; description->services[s] != NULL; s++) {
		service = client_service_init(desc, advertisement->location, description->services[s]);
		if (service != NULL) {
			list_add(&service->head, &device->services);
			if (client_service_subscribe(client, service) != 0) {
				debugf("client_service_subscribe(service); failed");
				client_device_uninit(device);
				goto out;
			}
		}
	}
	list_add_tail(&device->head, &client->devices);
	debugf("added '%s' to device list", device->uuid);
out:	free(uuid);
	free(devicetype);
	free(friendlyname);
	free(buffer);
	ixmlDocument_free(desc);
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
	pthread_mutex_lock(&client->mutex);
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
	pthread_mutex_unlock(&client->mutex);
	return 0;
}

static void * client_timer (void *arg)
{
	int stamp;
	client_t *client;
	struct timeval tval;
	struct timespec tspec;
	client_device_t *device;
	client_device_t *devicen;

	client = (client_t *) arg;
	stamp = 30;

	pthread_mutex_lock(&client->mutex);
	debugf("started timer thread");
	client->timer_running = 1;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);

	while (1) {
		pthread_mutex_lock(&client->mutex);
		gettimeofday(&tval, NULL);
		tspec.tv_sec = tval.tv_sec + stamp;
		tspec.tv_nsec = 0;
		pthread_cond_timedwait(&client->cond, &client->mutex, &tspec);
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
				debugf("sending search request for '%s'", device->uuid);
				if (upnp_search(upnp, 5, device->uuid) != 0) {
					debugf("error sending search request for %s", device->uuid);
				}
			}
		}
		pthread_mutex_unlock(&client->mutex);
	}
out:	client->timer_running = 0;
	debugf("stopped timer thread");
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);

	return NULL;
}

int client_init (client_t *client)
{
	int ret;
	ret = -1;
	debugf("initializing client '%s'", client->name);
	if (pthread_mutex_init(&client->mutex, NULL) != 0) {
		debugf("pthread_mutex_init(&client->mutex, NULL) failed");
		goto out;
	}
	if (pthread_cond_init(&client->cond, NULL) != 0) {
		debugf("pthread_cond_init(&client->cond, NULL) failed");
		pthread_mutex_destroy(&client->mutex);
		goto out;
	}
	debugf("initializing devices list");
	list_init(&client->devices);
	debugf("initializing upnp stack");
	upnp = upnp_init(client->interface, 0);
	if (upnp == NULL) {
		debugf("upnp_init() failed");
		pthread_cond_destroy(&client->cond);
		pthread_mutex_destroy(&client->mutex);
		goto out;
	}
	client->port = upnp_getport(upnp);
	client->ipaddress = upnp_getaddress(upnp);
	debugf("registering client device '%s'", client->name);
	if (upnp_register_client(upnp, client_event_handler, client)) {
		upnp_uninit(upnp);
		pthread_cond_destroy(&client->cond);
		pthread_mutex_destroy(&client->mutex);
		goto out;
	}
	client->running = 1;
	debugf("started client '%s'\n"
	       "  ipaddress  : %s\n"
	       "  port       : %d",
	       client->name,
	       client->ipaddress,
	       client->port);

	pthread_mutex_lock(&client->mutex);
	debugf("starting timer thread");
	pthread_create(&client->timer_thread, NULL, client_timer, client);
	while (client->timer_running == 0) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	debugf("timer is up and running");
	pthread_mutex_unlock(&client->mutex);

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
	pthread_mutex_lock(&client->mutex);
	client->running = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	debugf("waiting for timer thread to finish");
	pthread_mutex_lock(&client->mutex);
	while (client->timer_running != 0) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	debugf("joining timer thread");
	pthread_join(client->timer_thread, NULL);
	list_for_each_entry_safe(device, devicen, &client->devices, head) {
		list_del(&device->head);
		client_device_uninit(device);
	}
	debugf("unregistering client '%s'", client->name);
	upnp_uninit(upnp);
	pthread_mutex_unlock(&client->mutex);
	pthread_cond_destroy(&client->cond);
	pthread_mutex_destroy(&client->mutex);
	debugf("uninitialized client '%s'", client->name);
	return 0;
}

int client_refresh (client_t *client, int remove)
{
	int d;
	int ret;
	client_device_t *device;
	client_device_t *devicen;
	device_description_t *description;
	ret = 0;
	pthread_mutex_lock(&client->mutex);
	debugf("refreshing device list");
	if (remove != 0) {
		debugf("cleaning device list");
		list_for_each_entry_safe(device, devicen, &client->devices, head) {
			list_del(&device->head);
			client_device_uninit(device);
		}
	}
	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		if (upnp_search(upnp, 5, description->device) != 0) {
			debugf("error sending search request for %s", description->device);
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return ret;
}

IXML_Document * client_action (client_t *client, char *devicename, char *servicetype, char *actionname, char **param_name, char **param_val, int param_count)
{
	int rc;
	IXML_Document *response;
	IXML_Document *actionNode;
	client_device_t *device;
	client_service_t *service;

	rc = 0;
	response = NULL;
	actionNode = NULL;

	pthread_mutex_lock(&client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		if (strcmp(device->name, devicename) == 0) {
			goto found_device;
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return NULL;

found_device:
	list_for_each_entry(service, &device->services, head) {
		if (strcmp(service->type, servicetype) == 0) {
			goto found_service;
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return NULL;

found_service:
	response = upnp_makeaction(upnp, actionname, service->controlurl, service->type, param_count, param_name, param_val);
	if (response == NULL) {
		debugf("upnp_makeaction() failed");
		pthread_mutex_unlock(&client->mutex);
		return NULL;
	}
	pthread_mutex_unlock(&client->mutex);

	return response;
}
