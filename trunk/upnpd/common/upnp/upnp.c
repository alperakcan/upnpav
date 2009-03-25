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

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct upnp_s {
	char *host;
	unsigned short port;
	ssdp_t *ssdp;
	gena_t *gena;
	char *description;
	char *location;
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
		info->size = strlen(upnp->description);
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
		file->size = strlen(upnp->description);
		file->buf = strdup(upnp->description);
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

	char *deviceusn;

	pthread_mutex_lock(&upnp->mutex);
	upnp->description = strdup(description);
	if (upnp->description == NULL) {
		pthread_mutex_unlock(&upnp->mutex);
		return -1;
	}
	if (asprintf(&upnp->location, "http://%s:%d/description.xml", upnp->host, upnp->port) < 0) {
		free(upnp->description);
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
				ssdp_register(upnp->ssdp, "upnp:rootdevice", deviceusn, upnp->location, "mini upnp stack 1.0", 100000);
				free(deviceusn);
			}
			ssdp_register(upnp->ssdp, deviceudn, deviceudn, upnp->location, "mini upnp stack 1.0", 100000);
			if (asprintf(&deviceusn, "%s::%s", deviceudn, devicetype) > 0) {
				ssdp_register(upnp->ssdp, devicetype, deviceusn, upnp->location, "mini upnp stack 1.0", 100000);
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
				if (servicetype == NULL) {
					goto __continue;
				}
				if (asprintf(&deviceusn, "%s::%s", deviceudn, servicetype) > 0) {
					ssdp_register(upnp->ssdp, servicetype, deviceusn, upnp->location, "mini upnp stack 1.0", 100000);
					free(deviceusn);
				}
__continue:
				if (servicetype) {
					free(servicetype);
				}
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

	upnp->gena_callbacks.info = gena_callback_info,
	upnp->gena_callbacks.open = gena_callback_open,
	upnp->gena_callbacks.read = gena_callback_read,
	upnp->gena_callbacks.write = gena_callback_write,
	upnp->gena_callbacks.seek = gena_callback_seek,
	upnp->gena_callbacks.close = gena_callback_close,
	upnp->gena_callbacks.cookie = upnp;

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
	gena_uninit(upnp->gena);
	ssdp_uninit(upnp->ssdp);
	free(upnp->description);
	free(upnp->location);
	free(upnp->host);
	free(upnp);
	return 0;
}
