/***************************************************************************
    begin                : Mon Mar 02 2009
    copyright            : (C) 2009 by Alper Akcan
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
#include <string.h>
#include <pthread.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>

#include <upnp/ixml.h>

#include "upnp.h"
#include "list.h"
#include "uuid/uuid.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define debugf(a...) { \
	printf(a); \
	printf(" [%s (%s:%d)]\n", __FUNCTION__, __FILE__, __LINE__); \
}

#define UPNP_SUBSCRIPTION_MAX 100

typedef struct upnp_subscribe_s {
	list_t head;
	char *sid;
} upnp_subscribe_t;

typedef struct upnp_service_s {
	list_t head;
	char *event;
	list_t subscribers;
} upnp_service_t;

typedef struct upnp_device_s {
	char *description;
	char *location;
	list_t services;
} upnp_device_t;

struct upnp_s {
	char *host;
	unsigned short port;
	ssdp_t *ssdp;
	gena_t *gena;
	upnp_device_t device;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	gena_callbacks_t gena_callbacks;
};

static int gena_callback_info (void *cookie, char *path, gena_fileinfo_t *info)
{
	upnp_t *upnp;
	upnp = (upnp_t *) cookie;
	pthread_mutex_lock(&upnp->mutex);
	if (strcmp(path, "/description.xml") == 0) {
		info->size = strlen(upnp->device.description);
		info->mtime = 0;
		info->mimetype = strdup("text/xml");
		pthread_mutex_unlock(&upnp->mutex);
		return 0;
	}
	pthread_mutex_unlock(&upnp->mutex);
	return -1;
}

static void * gena_callback_open (void *cookie, char *path, gena_filemode_t mode)
{
	gena_file_t *file;
	upnp_t *upnp;
	upnp = (upnp_t *) cookie;
	pthread_mutex_lock(&upnp->mutex);
	if (strcmp(path, "/description.xml") == 0) {
		file = (gena_file_t *) malloc(sizeof(gena_file_t));
		if (file == NULL) {
			pthread_mutex_unlock(&upnp->mutex);
			return file;
		}
		memset(file, 0, sizeof(gena_file_t));
		file->virtual = 1;
		file->size = strlen(upnp->device.description);
		file->buf = strdup(upnp->device.description);
		if (file->buf == NULL) {
			free(file);
			file = NULL;
		}
		pthread_mutex_unlock(&upnp->mutex);
		return file;
	}
	pthread_mutex_unlock(&upnp->mutex);
	return NULL;
}

static int gena_callback_read (void *cookie, void *handle, char *buffer, unsigned int length)
{
	size_t len;
	gena_file_t *file;
	file = (gena_file_t *) handle;
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
	return -1;
}

static int gena_callback_write (void *cookie, void *handle, char *buffer, unsigned int length)
{
	return 0;
}

static unsigned long gena_callback_seek (void *cookie, void *handle, long offset, gena_seek_t whence)
{
	gena_file_t *file;
	file = (gena_file_t *) handle;
	if (file == NULL) {
		return -1;
	}
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
	return -1;
}

static int gena_callback_close (void *cookie, void *handle)
{
	gena_file_t *file;
	file = (gena_file_t *) handle;
	if (file == NULL) {
		return -1;
	}
	if (file->virtual == 1) {
		free(file->buf);
		free(file);
		return 0;
	}
	return 0;
}

static int gena_callback_event_subscribe (upnp_t *upnp, gena_event_subscribe_t *subscribe)
{
	int ret;
	upnp_service_t *s;
	ret = -1;
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->device.services, head) {
		if (strcmp(subscribe->path, s->event) == 0) {
			ret = -1;
			goto out;
		}
	}
out:	pthread_mutex_unlock(&upnp->mutex);
	return ret;
}

static int gena_callback_event (void *cookie, gena_event_t *event)
{
	upnp_t *upnp;
	upnp = (upnp_t *) cookie;
	switch (event->type) {
		case GENA_EVENT_TYPE_SUBSCRIBE:
			return gena_callback_event_subscribe(upnp, &event->event.subscribe);
		default:
			break;
	}
	return -1;
}

int upnp_advertise (upnp_t *upnp)
{
	int rc;
	char *location;
	pthread_mutex_lock(&upnp->mutex);
	if (asprintf(&location, "http://%s:%d/description.xml", upnp->host, upnp->port) < 0) {
		pthread_mutex_unlock(&upnp->mutex);
		return -1;
	}
	rc = ssdp_advertise(upnp->ssdp);
	free(location);
	pthread_mutex_unlock(&upnp->mutex);
	return rc;
}

static char * upnp_ixml_getfirstelementitem (IXML_Element *element, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	const char *val;
	char *ret = NULL;

	nodeList = ixmlElement_getElementsByTagName(element, (char *) item);
	if (nodeList == NULL) {
		return NULL;
	}
	if ((tmpNode = ixmlNodeList_item(nodeList, 0)) == NULL) {
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	textNode = ixmlNode_getFirstChild(tmpNode);
	val = ixmlNode_getNodeValue(textNode);
	if (val != NULL) {
		ret = strdup(val);
	}
	ixmlNodeList_free(nodeList);

	return ret;
}

int upnp_register_device (upnp_t *upnp, const char *description)
{
	int i;
	int j;
	int rc;

	IXML_Node *node;
	IXML_Document *desc;

	IXML_Node *devicenode;
	IXML_NodeList *devicelist;

	IXML_Node *servicenode;
	IXML_NodeList *services;
	IXML_NodeList *servicelist;

	char *devicetype;
	char *deviceudn;

	char *servicetype;
	char *eventurl;

	char *deviceusn;

	upnp_service_t *service;

	pthread_mutex_lock(&upnp->mutex);
	list_init(&upnp->device.services);
	upnp->device.description = strdup(description);
	if (upnp->device.description == NULL) {
		pthread_mutex_unlock(&upnp->mutex);
		return -1;
	}
	if (asprintf(&upnp->device.location, "http://%s:%d/description.xml", upnp->host, upnp->port) < 0) {
		free(upnp->device.description);
		pthread_mutex_unlock(&upnp->mutex);
		return -1;
	}

	rc = ixmlParseBufferEx(description, &desc);
	if (rc != IXML_SUCCESS) {
		pthread_mutex_unlock(&upnp->mutex);
		return -1;
	}

	devicelist = ixmlDocument_getElementsByTagName(desc, "device");
	if (devicelist != NULL) {
		for (i = 0; ; i++) {
			devicenode = NULL;
			devicetype = NULL;
			deviceudn = NULL;

			devicenode = ixmlNodeList_item(devicelist, i);
			if (devicenode == NULL) {
				break;
			}
			devicetype = upnp_ixml_getfirstelementitem((IXML_Element *) devicenode, "deviceType");
			if (devicetype == NULL) {
				goto _continue;
			}
			deviceudn = upnp_ixml_getfirstelementitem((IXML_Element *) devicenode, "UDN");
			if (deviceudn == NULL) {
				goto _continue;
			}

			/* ssdp entries for device */
			if (asprintf(&deviceusn, "%s::%s", deviceudn, "upnp:rootdevice") > 0) {
				ssdp_register(upnp->ssdp, "upnp:rootdevice", deviceusn, upnp->device.location, "mini upnp stack 1.0", 100000);
				free(deviceusn);
			}
			ssdp_register(upnp->ssdp, deviceudn, deviceudn, upnp->device.location, "mini upnp stack 1.0", 100000);
			if (asprintf(&deviceusn, "%s::%s", deviceudn, devicetype) > 0) {
				ssdp_register(upnp->ssdp, devicetype, deviceusn, upnp->device.location, "mini upnp stack 1.0", 100000);
				free(deviceusn);
			}

			services = ixmlElement_getElementsByTagName((IXML_Element *) devicenode, "service");
			if (services == NULL) {
				goto _continue;
			}
			for (j = 0; ; j++) {
				servicenode = NULL;

				servicenode = ixmlNodeList_item(services, j);
				if (servicenode == NULL) {
					break;
				}
				servicetype = upnp_ixml_getfirstelementitem((IXML_Element *) servicenode, "serviceType");
				eventurl = upnp_ixml_getfirstelementitem((IXML_Element *) servicenode, "eventSubURL");
				if (servicetype == NULL || eventurl == NULL) {
					goto __continue;
				}
				if (asprintf(&deviceusn, "%s::%s", deviceudn, servicetype) > 0) {
					if (ssdp_register(upnp->ssdp, servicetype, deviceusn, upnp->device.location, "mini upnp stack 1.0", 100000) == 0) {
						service = (upnp_service_t *) malloc(sizeof(upnp_service_t));
						if (service != NULL) {
							memset(service, 0, sizeof(upnp_service_t));
							list_init(&service->head);
							list_init(&service->subscribers);
							service->event = eventurl;
							list_add(&service->head, &upnp->device.services);
							eventurl = NULL;
						}
					}
					free(deviceusn);
				}
__continue:
				free(eventurl);
				free(servicetype);
			}
			if (services) {
				ixmlNodeList_free(services);
			}
_continue:
			if (devicetype) {
				free(devicetype);
			}
			if (deviceudn) {
				free(deviceudn);
			}
		}
	}

	servicelist = ixmlDocument_getElementsByTagName(desc, "serviceList");
	if (servicelist != NULL) {
		for (i = 0; ; i++) {
			node = ixmlNodeList_item(servicelist, i);
			if (node == NULL) {
				break;
			}
		}
	}

	ixmlNodeList_free(servicelist);
	ixmlNodeList_free(devicelist);
	ixmlDocument_free(desc);

	pthread_mutex_unlock(&upnp->mutex);
	return 0;
}

char * upnp_getaddress (upnp_t *upnp)
{
	return upnp->host;
}

unsigned short upnp_getport (upnp_t *upnp)
{
	return upnp->port;
}

upnp_t * upnp_init (const char *host, const unsigned short port)
{
	upnp_t *upnp;
	upnp = (upnp_t *) malloc(sizeof(upnp_t));
	if (upnp == NULL) {
		return NULL;
	}
	memset(upnp, 0, sizeof(upnp_t));
	upnp->host = strdup(host);
	if (upnp->host == NULL) {
		free(upnp);
		return NULL;
	}
	upnp->port = port;
	upnp->ssdp = ssdp_init();
	if (upnp->ssdp == NULL) {
		free(upnp->host);
		free(upnp);
		return NULL;
	}

	upnp->gena_callbacks.vfs.info = gena_callback_info;
	upnp->gena_callbacks.vfs.open = gena_callback_open;
	upnp->gena_callbacks.vfs.read = gena_callback_read;
	upnp->gena_callbacks.vfs.write = gena_callback_write;
	upnp->gena_callbacks.vfs.seek = gena_callback_seek;
	upnp->gena_callbacks.vfs.close = gena_callback_close;
	upnp->gena_callbacks.vfs.cookie = upnp;

	upnp->gena_callbacks.gena.event = gena_callback_event;
	upnp->gena_callbacks.gena.cookie = upnp;

	upnp->gena = gena_init(upnp->host, upnp->port, &upnp->gena_callbacks);
	if (upnp->gena == NULL) {
		ssdp_uninit(upnp->ssdp);
		free(upnp->host);
		free(upnp);
		return NULL;
	}
	upnp->port = gena_getport(upnp->gena);

	pthread_mutex_init(&upnp->mutex, NULL);
	pthread_cond_init(&upnp->cond, NULL);
	return upnp;
}

int upnp_uninit (upnp_t *upnp)
{
	upnp_service_t *s;
	upnp_service_t *sn;
	upnp_subscribe_t *c;
	upnp_subscribe_t *cn;
	list_for_each_entry_safe(s, sn, &upnp->device.services, head) {
		list_for_each_entry_safe(c, cn, &s->subscribers, head) {
			list_del(&c->head);
			free(c->sid);
			free(c);
		}
		list_del(&s->head);
		free(s->event);
		free(s);
	}
	gena_uninit(upnp->gena);
	ssdp_uninit(upnp->ssdp);
	free(upnp->device.description);
	free(upnp->device.location);
	free(upnp->host);
	free(upnp);
	return 0;
}
