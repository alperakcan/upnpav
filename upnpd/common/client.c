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

static int client_variable_uninit (client_variable_t *variable)
{
	free(variable->name);
	free(variable->value);
	free(variable);
	return 0;
}

static client_variable_t * client_variable_init (char *name, char *value)
{
	client_variable_t *variable;
	if (name == NULL) {
		return NULL;
	}
	variable = (client_variable_t *) malloc(sizeof(client_variable_t));
	if (variable == NULL) {
		return NULL;
	}
	variable->name = strdup(name);
	variable->value = (value) ? strdup(value) : strdup("");
	if (variable->name == NULL ||
	    variable->value == NULL) {
		free(variable->name);
		free(variable->value);
		free(variable);
		return NULL;
	}
	return variable;
}

static int client_service_uninit (client_service_t *service)
{
	client_variable_t *variable;
	while (service->variables != NULL) {
		variable = service->variables;
		service->variables = variable->next;
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
			service->type = strdup(servicetype);
			service->id = xml_get_first_element_item(servicedesc, "serviceId");
			relcontrolURL = xml_get_first_element_item(servicedesc, "controlURL");
			releventURL = xml_get_first_element_item(servicedesc, "eventSubURL");
			service->controlurl = malloc(strlen(base) + strlen(relcontrolURL) + 1);
			if (service->controlurl) {
				ret = UpnpResolveURL(base, relcontrolURL, service->controlurl);
				if (ret != UPNP_E_SUCCESS) {
					debugf("error generating control url from '%s' + '%s'", base, relcontrolURL);
					free(service->controlurl);
					service->controlurl = NULL;
				}
			}
			service->eventurl = malloc(strlen(base) + strlen(releventURL) + 1);
			if (service->eventurl) {
				ret = UpnpResolveURL(base, releventURL, service->eventurl);
				if (ret != UPNP_E_SUCCESS) {
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
	while (device->services != NULL) {
		service = device->services;
		device->services = service->next;
		client_service_uninit(service);
	}
	free(device->name);
	free(device->type);
	free(device->uuid);
	free(device->friendlyname);
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
	device->friendlyname = strdup(friendlyname);
	device->name = strdup(friendlyname);
	device->expiretime = expiretime;
	if (device->type == NULL ||
	    device->uuid == NULL ||
	    device->friendlyname == NULL ||
	    device->name == NULL) {
		client_device_uninit(device);
		return NULL;
	}
	return device;
}

static int client_device_add (client_t *client, client_device_t *device)
{
	device->next = client->devices;
	if (device->next != NULL) {
		device->next->prev = device;
	}
	client->devices = device;
	return 0;
}

static int client_device_del (client_t *client, client_device_t *device)
{
	if (device->prev != NULL) {
		device->prev->next = device->next;
	} else {
		client->devices = device->next;
	}
	if (device->next != NULL) {
		device->next->prev = device->prev;
	}
	return 0;
}

static int client_service_add (client_device_t *device, client_service_t *service)
{
	service->next = device->services;
	if (service->next != NULL) {
		service->next->prev = service;
	}
	device->services = service;
	return 0;
}

static client_service_t * client_service_find (client_t *client, char *eventurl)
{
	client_device_t *device;
	client_service_t *service;
	device = client->devices;
	while (device != NULL) {
		service = device->services;
		while (service != NULL) {
			if (strcmp(service->eventurl, eventurl) == 0) {
				return service;
			}
			service = service->next;
		}
		device = device->next;
	}
	return NULL;
}

static int client_variable_add (client_service_t *service, client_variable_t *variable)
{
	variable->next = service->variables;
	if (variable->next != NULL) {
		variable->next->prev = variable;
	}
	service->variables = variable;
	return 0;
}

static int client_service_subscribe (client_t *client, client_service_t *service)
{
	int t;
	int rc;
	Upnp_SID sid;
	debugf("subscribing to '%s'", service->eventurl);
	t = 1801;
	rc = UpnpSubscribe(client->handle, service->eventurl, &t, sid);
	if (rc != UPNP_E_SUCCESS) {
		debugf("upnpsubscribe() failed");
		return -1;
	}
	service->sid = strdup(sid);
	if (service->sid == NULL) {
		return -1;
	}
	return 0;
}

static int client_event_search_result (client_t *client, struct Upnp_Discovery *event)
{
	int d;
	int s;
	int rc;
	client_device_t *device;
	client_service_t *service;
	device_description_t *description;

	char *uuid = NULL;
	char *devicetype = NULL;
	char *friendlyname = NULL;
	IXML_Document *desc = NULL;

	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		if (strcmp(event->DeviceType, description->device) == 0) {
			goto found;
		}
	}
	return 0;
found:
	device = client->devices;
	while (device != NULL) {
		if (strcmp(event->DeviceId, device->uuid) == 0) {
			device->expiretime = event->Expires;
			return 0;
		}
		device = device->next;
	}
	rc = UpnpDownloadXmlDoc(event->Location, &desc);
	if (rc != UPNP_E_SUCCESS) {
		debugf("UpnpDownloadXmlDoc(%s, &desc) failed (%d)", event->Location, rc);
		return 0;
	}
	debugf("reding elements from document '%s'", event->Location);
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
	device = client_device_init(devicetype, uuid, friendlyname, event->Expires);
	if (device == NULL) {
		goto out;
	}
	for (s = 0; description->services[s] != NULL; s++) {
		service = client_service_init(desc, event->Location, description->services[s]);
		if (service != NULL) {
			client_service_add(device, service);
			if (client_service_subscribe(client, service) != 0) {
				debugf("client_service_subscribe(service); failed");
				client_device_uninit(device);
				goto out;
			}
		}
	}
	client_device_add(client, device);
	debugf("added '%s' to device list", device->uuid);
out:	free(uuid);
	free(devicetype);
	free(friendlyname);
	ixmlDocument_free(desc);
	return 0;
}

static int client_event_advertisement_byebye (client_t *client, struct Upnp_Discovery *event)
{
	int d;
	client_device_t *device;
	device_description_t *description;
	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		if (strcmp(event->DeviceType, description->device) == 0) {
			goto found;
		}
	}
	return 0;
found:
	device = client->devices;
	while (device != NULL) {
		if (strcmp(event->DeviceId, device->uuid) == 0) {
			client_device_del(client, device);
			debugf("removed '%s' from device list", device->uuid);
			client_device_uninit(device);
			return 0;
		}
		device = device->next;
	}
	return 0;
}

static int client_event_state_update (client_service_t *service, IXML_Document *ChangedVariables)
{
	IXML_NodeList *properties, *variables;
	IXML_Element *property, *variable;
	int length, length1;
	int i, j, f;
	char *value;
	char *name;
	client_variable_t *var;

	debugf("state update (service '%s'):", service->type);

	properties = ixmlDocument_getElementsByTagName(ChangedVariables, "e:property");
	if (NULL != properties) {
		length = ixmlNodeList_length(properties);
		for (i = 0; i < length; i++) {
			property = (IXML_Element *) ixmlNodeList_item(properties, i);
			variables = ixmlElement_getElementsByTagName(property, "*");
			if (variables) {
				length1 = ixmlNodeList_length(variables);
				for (j = 0; j < length1; j++ ) {
					variable = (IXML_Element *) ixmlNodeList_item(variables, j);
					name = (char *) ixmlElement_getTagName(variable);
					if (name && strcmp (name, "e:property") != 0) {
						value = xml_get_element_value(variable);
						debugf("variable update '%s' = '%s'", name, (value) ? value : "");
						for (f = 0, var = service->variables; var != NULL; var = var->next) {
							if (strcmp(var->name, name) == 0) {
								free(var->value);
								var->value = (value) ? strdup(value) : strdup("");
								debugf("updated variable '%s'='%s'", var->name, var->value);
								f = 1;
								break;
							}
						}
						if (f == 0) {
							var = client_variable_init(name, value);
							if (var != NULL) {
								client_variable_add(service, var);
								debugf("added new variable '%s'='%s'", var->name, var->value);
							}
						}
						free(value);
					}
				}
			}
			ixmlNodeList_free(variables);
			variables = NULL;
		}
	}
        ixmlNodeList_free(properties);
        return 0;
}

static int client_event_event_received (client_t *client, struct Upnp_Event *event)
{
	client_device_t *device;
	client_service_t *service;

	for (device = client->devices; device != NULL; device = device->next) {
		for (service = device->services; service != NULL; service = service->next) {
			if (strcmp(service->sid, event->Sid) == 0) {
				debugf("received '%s' event '%d' for '%s'", service->id, event->EventKey, service->sid);
				client_event_state_update(service, event->ChangedVariables);
			}
		}
	}

	return 0;
}

static int client_event_event_subscription_expired (client_t *client, struct Upnp_Event_Subscribe *event)
{
	client_service_t *service;
	service = client_service_find(client, event->PublisherUrl);
	if (service == NULL) {
		debugf("client_service_find(client, %s); failed", event->PublisherUrl);
		return -1;
	}
	return client_service_subscribe(client, service);
}

static int client_event_handler (Upnp_EventType eventtype, void *event, void *cookie)
{
	client_t *client;
	client = (client_t *) cookie;
	pthread_mutex_lock(&client->mutex);
	switch (eventtype) {
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	        case UPNP_DISCOVERY_SEARCH_RESULT:
	        	client_event_search_result(client, event);
	        	break;
	        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
	        	/* nothing to do */
	        	break;
	        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	        	client_event_advertisement_byebye(client, event);
	        	break;
	        case UPNP_CONTROL_ACTION_COMPLETE:
	        	assert(0);
	        	break;
	        case UPNP_CONTROL_GET_VAR_COMPLETE:
	        	assert(0);
	        	break;
	        case UPNP_EVENT_RECEIVED:
	        	client_event_event_received(client, event);
	        	break;
	        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
	        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
	        case UPNP_EVENT_RENEWAL_COMPLETE:
	        	assert(0);
	        	break;
	        case UPNP_EVENT_AUTORENEWAL_FAILED:
	        case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
	        	client_event_event_subscription_expired(client, event);
	        	break;
	        /* ignore below */
	        case UPNP_EVENT_SUBSCRIPTION_REQUEST:
	        case UPNP_CONTROL_GET_VAR_REQUEST:
	        case UPNP_CONTROL_ACTION_REQUEST:
	        	break;
	}
	pthread_mutex_unlock(&client->mutex);
	return 0;
}

static void * client_timer (void *arg)
{
	int rc;
	int stamp;
	client_t *client;
	client_device_t *tmp;
	client_device_t *device;
	struct timespec tspec;

	client = (client_t *) arg;
	stamp = 30;

	pthread_mutex_lock(&client->mutex);
	debugf("started timer thread");
	client->timer_running = 1;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);

	while (1) {
		pthread_mutex_lock(&client->mutex);
		clock_gettime(CLOCK_REALTIME, &tspec);
		tspec.tv_sec += stamp;
		pthread_cond_timedwait(&client->cond, &client->mutex, &tspec);
		if (client->running == 0) {
			goto out;
		}
		debugf("checking for expire times");
		device = client->devices;
		while (device != NULL) {
			device->expiretime -= stamp;
			tmp = device;
			device = device->next;
			if (tmp->expiretime < 0) {
				client_device_del(client, tmp);
				debugf("removed '%s' from device list", tmp->uuid);
				client_device_uninit(tmp);
			} else if (tmp->expiretime < stamp) {
				debugf("sending search request for '%s'", tmp->uuid);
				rc = UpnpSearchAsync(client->handle, 5, tmp->uuid, client);
				if (UPNP_E_SUCCESS != rc) {
					debugf("error sending search request for %s (%d)", tmp->uuid, rc);
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
	int rc;
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
	debugf("initilizing upnp stack");
	if ((rc = UpnpInit((client->interface) ? client->interface : NULL, 0)) != UPNP_E_SUCCESS) {
		debugf("UpnpInit(NULL, 0) failed (%d:%s)", rc, UpnpGetErrorMessage(rc));
		pthread_cond_destroy(&client->cond);
		pthread_mutex_destroy(&client->mutex);
		UpnpFinish();
		goto out;
	}
	client->port = UpnpGetServerPort();
	client->ipaddress = UpnpGetServerIpAddress();
	debugf("registering client device '%s'", client->name);
	if ((rc = UpnpRegisterClient(client_event_handler, client, &client->handle)) != UPNP_E_SUCCESS) {
		debugf("UpnpRegisterClient() failed (%d:%s)", rc, UpnpGetErrorMessage(rc));
		pthread_cond_destroy(&client->cond);
		pthread_mutex_destroy(&client->mutex);
		UpnpFinish();
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
	while (client->devices != NULL) {
		device = client->devices;
		client->devices = device->next;
		client_device_uninit(device);
	}
	debugf("unregistering client '%s'", client->name);
	UpnpUnRegisterClient(client->handle);
	UpnpFinish();
	pthread_mutex_unlock(&client->mutex);
	pthread_cond_destroy(&client->cond);
	pthread_mutex_destroy(&client->mutex);
	debugf("uninitialized client '%s'", client->name);
	return 0;
}

int client_refresh (client_t *client, int remove)
{
	int rc;
	int d;
	int ret;
	client_device_t *tmp;
	client_device_t *device;
	device_description_t *description;
	ret = 0;
	pthread_mutex_lock(&client->mutex);
	debugf("refreshing device list");
	if (remove != 0) {
		debugf("cleaning device list");
		device = client->devices;
		while (device != NULL) {
			tmp = device;
			device = device->next;
			client_device_uninit(tmp);
		}
		client->devices = NULL;
	}
	for (d = 0; (description = client->descriptions[d]) != NULL; d++) {
		rc = UpnpSearchAsync(client->handle, 5, description->device, client);
		if (UPNP_E_SUCCESS != rc) {
			debugf("error sending search request for %s (%d)", description->device, rc);
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return ret;
}

IXML_Document * client_action (client_t *client, char *devicename, char *servicetype, char *actionname, char **param_name, char **param_val, int param_count)
{
	int rc;
	int param;
	IXML_Document *response;
	IXML_Document *actionNode;
	client_device_t *device;
	client_service_t *service;

	rc = 0;
	response = NULL;
	actionNode = NULL;

	pthread_mutex_lock(&client->mutex);

	for (device = client->devices; device != NULL; device = device->next) {
		if (strcmp(device->name, devicename) == 0) {
			goto found_device;
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return NULL;
found_device:
	for (service = device->services; service != NULL; service = service->next) {
		if (strcmp(service->type, servicetype) == 0) {
			goto found_service;
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return NULL;
found_service:

	if (param_count == 0) {
		actionNode = UpnpMakeAction(actionname, service->type, 0, NULL);
	} else {
		for (param = 0; param < param_count; param++) {
			if (UpnpAddToAction(&actionNode, actionname, service->type, param_name[param], param_val[param]) != UPNP_E_SUCCESS) {
				debugf("error while trying to add action param\n");
				pthread_mutex_unlock(&client->mutex);
				return NULL;
			}
		}
	}

	rc = UpnpSendAction(client->handle, service->controlurl, service->type, NULL, actionNode, &response);
	if (rc != UPNP_E_SUCCESS) {
		debugf("error in UpnpSendAction (%d:%s) (%s, %s, %p)", rc, UpnpGetErrorMessage(rc), service->controlurl, service->type, actionNode);
		if (response != NULL) {
			ixmlDocument_free(response);
			response = NULL;
		}
	}
	if (actionNode) {
		ixmlDocument_free(actionNode);
	}
	pthread_mutex_unlock(&client->mutex);
	return response;
}
