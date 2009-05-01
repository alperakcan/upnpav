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


#include "upnp.h"
#include "ssdp.h"
#include "gena.h"
#include "ixml.h"
#include "list.h"
#include "uuid.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define debugf(a...) { \
	printf(a); \
	printf(" [%s (%s:%d)]\n", __FUNCTION__, __FILE__, __LINE__); \
}

#define UPNP_SUBSCRIPTION_MAX 100

typedef struct upnp_error_s {
	int code;
	char *str;
} upnp_error_t;

static upnp_error_t upnp_errors[] = {
	[UPNP_ERROR_INVALID_ACTION]             = {401, "Invalid Action"},
	[UPNP_ERROR_INVALIG_ARGS]               = {402, "Invalid Args"},
	[UPNP_ERROR_INVALID_VAR]                = {404, "Invalid Var"},
	[UPNP_ERROR_ACTION_FAILED]              = {501, "Action Failed"},
	[UPNP_ERROR_NOSUCH_OBJECT]              = {701, "No Such Object"},
	[UPNP_ERROR_INVALID_CURRENTTAG]         = {702, "Invalid CurrentTag Value"},
	[UPNP_ERROR_INVALID_NEWTAG]             = {703, "Invalid NewTag Value"},
	[UPNP_ERROR_REQUIRED_TAG]               = {704, "Required Tag"},
	[UPNP_ERROR_READONLY_TAG]               = {705, "Read only Tag"},
	[UPNP_ERROR_PARAMETER_MISMATCH]         = {706, "Parameter Mismatch"},
	[UPNP_ERROR_INVALID_SEARCH_CRITERIA]    = {708, "Unsupported or Invalid Search Criteria"},
	[UPNP_ERROR_INVALID_SORT_CRITERIA]      = {709, "Unsupported or Invalid Sort Criteria"},
	[UPNP_ERROR_NOSUCH_CONTAINER]           = {710, "No Such Container"},
	[UPNP_ERROR_RESTRICTED_OBJECT]          = {711, "Restricted Object"},
	[UPNP_ERROR_BAD_METADATA]               = {712, "Bad Metadata"},
	[UPNP_ERROR_RESTRICTED_PARENT_OBJECT]   = {713, "Restricted Parent Object"},
	[UPNP_ERROR_NOSUCH_RESOURCE]            = {714, "No Such Source Resouce"},
	[UPNP_ERROR_RESOURCE_ACCESS_DENIED]     = {715, "Source Resource Access Denied"},
	[UPNP_ERROR_TRANSFER_BUSY]              = {716, "Transfer Busy"},
	[UPNP_ERROR_NOSUCH_TRANSFER]            = {717, "No Such File Transfer"},
	[UPNP_ERROR_NOSUCH_DESTRESOURCE]        = {718, "No Such Destination Resource"},
	[UPNP_ERROR_INVALID_INSTANCEID]         = {718, "Invalid InstanceID"},
	[UPNP_ERROR_DESTRESOURCE_ACCESS_DENIED] = {719, "Destination Resource Access Denied"},
	[UPNP_ERROR_CANNOT_PROCESS]             = {720, "Cannot Process The Request"},
};

typedef struct upnp_subscribe_s {
	list_t head;
	char sid[50];
	unsigned int sequence;
	upnp_url_t url;
} upnp_subscribe_t;

typedef struct upnp_service_s {
	list_t head;
	char *udn;
	char *serviceid;
	char *eventurl;
	char *controlurl;
	list_t subscribers;
} upnp_service_t;

typedef enum {
	UPNP_TYPE_UNKNOWN = 0x00,
	UPNP_TYPE_DEVICE,
	UPNP_TYPE_CLIENT,
} upnp_type_t;

typedef struct upnp_device_s {
	upnp_type_t type;
	char *description;
	char *location;
	list_t services;
	int (*callback) (void *cookie, upnp_event_t *event);
	void *cookie;
} upnp_device_t;

typedef struct upnp_client_s {
	upnp_type_t type;
	int (*callback) (void *cookie, upnp_event_t *event);
	void *cookie;
} upnp_client_t;

struct upnp_s {
	char *host;
	unsigned short port;
	ssdp_t *ssdp;
	gena_t *gena;
	union {
		upnp_type_t type;
		upnp_device_t device;
		upnp_client_t client;
	} type;
	pthread_mutex_t mutex;
	gena_callbacks_t gena_callbacks;
};

static int gena_callback_info (void *cookie, char *path, gena_fileinfo_t *info)
{
	upnp_t *upnp;
	upnp = (upnp_t *) cookie;
	pthread_mutex_lock(&upnp->mutex);
	if (strcmp(path, "/description.xml") == 0) {
		info->size = strlen(upnp->type.device.description);
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
		file->size = strlen(upnp->type.device.description);
		file->buf = strdup(upnp->type.device.description);
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

int upnp_addtoactionresponse (upnp_event_action_t *response, const char *service, const char *variable, const char *value)
{
	int rc;
	char *buf;
	const char *fmt =
		"<u:%sResponse xmlns:u=\"%s\">\r\n"
		"</u:%sResponse>";

	IXML_Node *node = NULL;
	IXML_Node *text = NULL;
	IXML_Element *elem = NULL;

	if (service == NULL) {
		return -1;
	}
	if (response->response == NULL) {
		if (asprintf(&buf, fmt, response->action, service, response->action) < 0) {
			return -1;
		}
	        rc = ixmlParseBufferEx(buf, &response->response);
	        free(buf);
	        if (rc != IXML_SUCCESS) {
	        	return -1;
	        }
	}
	if (variable != NULL) {
		node = ixmlNode_getFirstChild((IXML_Node *) response->response);
		elem = ixmlDocument_createElement(response->response, variable);
	        if (value != NULL) {
	        	text = ixmlDocument_createTextNode(response->response, value);
	        	ixmlNode_appendChild((IXML_Node *) elem, text);
	        }
	        ixmlNode_appendChild(node, (IXML_Node *) elem);
	}
	return 0;
}

static char * upnp_propertyset (const char **names, const char **values, unsigned int count)
{
	char *buffer;
	int counter = 0;
	int size = 0;
	int temp_counter = 0;

	size += strlen("<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">\n");
	size += strlen("</e:propertyset>\n\n");

	for (temp_counter = 0, counter = 0; counter < count; counter++) {
		size += strlen( "<e:property>\n</e:property>\n" );
		size += (2 * strlen(names[counter]) + strlen(values[counter]) + (strlen("<></>\n")));
	}
	buffer = (char *)malloc(size + 1);
	if (buffer == NULL) {
		return NULL;
	}
	memset(buffer, 0, size + 1);

	strcpy(buffer, "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">\n");
	for (counter = 0; counter < count; counter++) {
		strcat(buffer, "<e:property>\n");
		sprintf(&buffer[strlen(buffer)], "<%s>%s</%s>\n</e:property>\n", names[counter], values[counter], names[counter]);
	}
	strcat(buffer, "</e:propertyset>\n\n");
	return buffer;
}

int upnp_accept_subscription (upnp_t *upnp, const char *udn, const char *serviceid, const char **variable_names, const char **variable_values, const unsigned int variables_count, const char *sid)
{
	char *data;
	char *header;
	char *propset;
	upnp_service_t *s;
	upnp_subscribe_t *c;
	const char *format =
		"NOTIFY %s HTTP/1.1\r\n"
		"HOST: %s:%d\r\n"
		"CONTENT-TYPE: text/xml\r\n"
		"CONTENT-LENGTH: %u\r\n"
		"NT: upnp:event\r\n"
		"NTS: upnp:propchange\r\n"
		"SID: %s\r\n"
		"SEQ: %u\r\n"
		"\r\n";
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(s->serviceid, serviceid) == 0 &&
		    strcmp(s->udn, udn) == 0) {
			list_for_each_entry(c, &s->subscribers, head) {
				if (strcmp(c->sid, sid) == 0) {
					goto found;
				}
			}
			break;
		}
	}
	goto out;

found:
	propset = upnp_propertyset(variable_names, variable_values, variables_count);
	if (propset == NULL) {
		goto out;
	}
	if (asprintf(&header, format,
			s->eventurl,
			c->url.host, c->url.port,
			strlen(propset),
			c->sid,
			c->sequence) < 0) {
		free(propset);
		goto out;
	}
	c->sequence++;

	printf("header: %s\n", header);
	printf("propset: %s\n", propset);

	data = gena_send_recv(upnp->gena, c->url.host, c->url.port, header, propset);
	if (data == NULL) {
		debugf("gena_send_recv() failed");
	}

	free(data);
	free(header);
	free(propset);
out:
	pthread_mutex_unlock(&upnp->mutex);
	return 0;
}

static size_t __strnlen (const char *string, size_t maxlen)
{
	const char *end = memchr(string, '\0', maxlen);
	return end ? (size_t) (end - string) : maxlen;
}

static char * self_strndup (const char *s, size_t n)
{
	size_t len = __strnlen(s, n);
	char *new = malloc(len + 1);
	if (new == NULL)
		return NULL;
	new[len] = '\0';
	return memcpy(new, s, len);
}

int upnp_url_uninit (upnp_url_t *url)
{
	free(url->host);
	free(url->path);
	free(url->url);
	url->host = NULL;
	url->url = NULL;
	url->port = 0;
	url->path = NULL;
	return 0;
}

int upnp_url_parse (const char *uri, upnp_url_t *url)
{
	char *i;
	char *p;
	char *e;
	url->url = strdup(uri);
	if (url->url == NULL) {
		return -1;
	}
	if (strncasecmp(url->url, "http://", 7) == 0) {
		i = url->url + 7;
	} else {
		i = url->url;
	}

	p = strchr(i, ':');
	e = strchr(i, '/');
	if (p == NULL || e < p) {
		url->port = 80;
		if (e == NULL) {
			url->host = strdup(i);
		} else {
			*e = '\0';
			url->host = self_strndup(i, e - i);
		}
	} else {
		if (e != NULL) {
			*e = '\0';
		}
		url->port = atoi(p + 1);
		url->host = self_strndup(i, p - i);
	}
	if (e != NULL) {
		do {
			e++;
		} while (*e == '/');
		url->path = strdup(e);
	}
	if (url->host == NULL || url->port == 0) {
		upnp_url_uninit(url);
		return -1;
	}
	return 0;
}

static int gena_callback_event_subscribe_request (upnp_t *upnp, gena_event_subscribe_t *subscribe)
{
	int ret;
	uuid_t uuid;
	upnp_service_t *s;
	upnp_subscribe_t *c;
	ret = -1;
	debugf("enter");
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		debugf("eventurl: %s", s->eventurl);
		if (strcmp(subscribe->path, s->eventurl) == 0) {
			c = (upnp_subscribe_t *) malloc(sizeof(upnp_subscribe_t));
			if (c == NULL) {
				ret = -1;
				goto out;
			}
			memset(c, 0, sizeof(upnp_subscribe_t));
			uuid_generate(uuid);
			sprintf(c->sid, "uuid:");
			uuid_unparse_lower(uuid, c->sid + strlen(c->sid));
			subscribe->sid = strdup(c->sid);
			if (subscribe->sid == NULL) {
				free(c);
				ret = -1;
				goto out;
			}
			if (upnp_url_parse(subscribe->callback + 1, &c->url) != 0) {
				free(c);
				ret = -1;
				goto out;
			}
			list_add_tail(&c->head, &s->subscribers);
			ret = 0;
			goto out;
		}
	}
out:	pthread_mutex_unlock(&upnp->mutex);
	debugf("ret: %d", ret);
	return ret;
}

static int gena_callback_event_subscribe_accept (upnp_t *upnp, gena_event_subscribe_t *subscribe)
{
	int ret;
	upnp_event_t e;
	upnp_service_t *s;
	ret = -1;
	debugf("enter");
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(subscribe->path, s->eventurl) == 0) {
			memset(&e, 0, sizeof(upnp_event_t));
			e.type = UPNP_EVENT_TYPE_SUBSCRIBE_REQUEST;
			e.event.subscribe.serviceid = s->serviceid;
			e.event.subscribe.udn = s->udn;
			e.event.subscribe.sid = subscribe->sid;
			pthread_mutex_unlock(&upnp->mutex);
			if (upnp->type.device.callback != NULL) {
				upnp->type.device.callback(upnp->type.device.cookie, &e);
			}
			pthread_mutex_lock(&upnp->mutex);
			ret = 0;
			goto out;
		}
	}
out:	pthread_mutex_unlock(&upnp->mutex);
	debugf("ret: %d", ret);
	return ret;
}

static int gena_callback_event_subscribe_renew (upnp_t *upnp, gena_event_subscribe_t *subscribe)
{
	int ret;
	upnp_service_t *s;
	upnp_subscribe_t *c;
	ret = -1;
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(subscribe->path, s->eventurl) == 0) {
			list_for_each_entry(c, &s->subscribers, head) {
				if (strcmp(c->sid, subscribe->sid) == 0) {
					ret = 0;
					goto out;
				}
			}
		}
	}
out:	pthread_mutex_unlock(&upnp->mutex);
	return -1;
}

static int gena_callback_event_subscribe_drop (upnp_t *upnp, gena_event_unsubscribe_t *unsubscribe)
{
	upnp_service_t *s;
	upnp_subscribe_t *c;
	upnp_subscribe_t *cn;
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(unsubscribe->path, s->eventurl) == 0) {
			list_for_each_entry_safe(c, cn, &s->subscribers, head) {
				if (strcmp(c->sid, unsubscribe->sid) == 0) {
					list_del(&c->head);
					free(c->url.host);
					free(c->url.url);
					free(c->url.path);
					free(c);
				}
			}
		}
	}
	pthread_mutex_unlock(&upnp->mutex);
	return 0;
}

static int gena_callback_event_action (upnp_t *upnp, gena_event_action_t *action)
{
	int ret;
	upnp_event_t e;
	upnp_service_t *s;
	char *response;
	const char *envelope =
	        "<s:Envelope "
	        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
	        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
	        "<s:Body>\n"
		"%s"
		"</s:Body>\n"
		"</s:Envelope>\n";
	const char *fault =
		"<s:Envelope "
		"xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
		"<s:Body>\n"
		"<s:Fault>\n"
		"<faultcode>s:Client</faultcode>\n"
		"<faultstring>UPnPError</faultstring>\n"
		"<detail>\n"
		"<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\n"
		"<errorCode>%d</errorCode>\n"
		"<errorDescription>%s</errorDescription>\n"
		"</UPnPError>\n"
		"</detail>\n"
		"</s:Fault>\n"
		"</s:Body>\n"
		"</s:Envelope>\n";

	ret = -1;
	pthread_mutex_lock(&upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(action->path, s->controlurl) == 0) {
			memset(&e, 0, sizeof(upnp_event_t));
			e.type = UPNP_EVENT_TYPE_ACTION;
			e.event.action.action = action->action;
			ret = ixmlParseBufferEx(action->request, &e.event.action.request);
			if (ret != IXML_SUCCESS) {
				ret = -1;
				goto out;
			}
			e.event.action.serviceid = s->serviceid;
			e.event.action.udn = s->udn;
			pthread_mutex_unlock(&upnp->mutex);
			if (upnp->type.device.callback != NULL) {
				upnp->type.device.callback(upnp->type.device.cookie, &e);
			}
			if (e.event.action.errcode == 0) {
				response = ixmlPrintNode((IXML_Node *) e.event.action.response);
				if (response != NULL) {
					if (asprintf(&action->response,
							envelope,
							response) < 0) {
						action->response = NULL;
					}
					free(response);
				}
			} else {
				if (asprintf(&action->response, fault, upnp_errors[e.event.action.errcode].code, upnp_errors[e.event.action.errcode].str) < 0) {
					action->response = NULL;
				}
			}
			ixmlDocument_free(e.event.action.request);
			ixmlDocument_free(e.event.action.response);
			pthread_mutex_lock(&upnp->mutex);
			ret = 0;
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
		case GENA_EVENT_TYPE_SUBSCRIBE_REQUEST:
			return gena_callback_event_subscribe_request(upnp, &event->event.subscribe);
		case GENA_EVENT_TYPE_SUBSCRIBE_ACCEPT:
			return gena_callback_event_subscribe_accept(upnp, &event->event.subscribe);
		case GENA_EVENT_TYPE_SUBSCRIBE_RENEW:
			return gena_callback_event_subscribe_renew(upnp, &event->event.subscribe);
		case GENA_EVENT_TYPE_SUBSCRIBE_DROP:
			return gena_callback_event_subscribe_drop(upnp, &event->event.unsubscribe);
		case GENA_EVENT_TYPE_ACTION:
			return gena_callback_event_action(upnp, &event->event.action);
		default:
			break;
	}
	return -1;
}

static int ssdp_callback_event_notify (upnp_t *upnp, ssdp_event_notify_t *notify)
{
	upnp_event_t e;
	memset(&e, 0, sizeof(upnp_event_t));
	e.type = UPNP_EVENT_TYPE_ADVERTISEMENT_ALIVE;
	e.event.advertisement.device = notify->device;
	e.event.advertisement.location = notify->location;
	e.event.advertisement.uuid = notify->uuid;
	e.event.advertisement.expires = notify->expires;
	if (upnp->type.client.callback != NULL) {
		upnp->type.client.callback(upnp->type.client.cookie, &e);
	}
	return 0;
}

static int ssdp_callback_event_byebye (upnp_t *upnp, ssdp_event_byebye_t *byebye)
{
	upnp_event_t e;
	memset(&e, 0, sizeof(upnp_event_t));
	e.type = UPNP_EVENT_TYPE_ADVERTISEMENT_BYEBYE;
	e.event.advertisement.device = byebye->device;
	e.event.advertisement.uuid = byebye->uuid;
	if (upnp->type.client.callback != NULL) {
		upnp->type.client.callback(upnp->type.client.cookie, &e);
	}
	return 0;
}

static int ssdp_callback_event (void *cookie, ssdp_event_t *event)
{
	upnp_t *upnp;
	upnp = (upnp_t *) cookie;
	if (upnp->type.type == UPNP_TYPE_DEVICE) {
		return 0;
	}
	switch (event->type) {
		case SSDP_EVENT_TYPE_NOTIFY:
			return ssdp_callback_event_notify(upnp, &event->event.notify);
		case SSDP_EVENT_TYPE_BYEBYE:
			return ssdp_callback_event_byebye(upnp, &event->event.byebye);
		default:
			break;
	}
	return -1;
}

int upnp_advertise (upnp_t *upnp)
{
	int rc;
	pthread_mutex_lock(&upnp->mutex);
	rc = ssdp_advertise(upnp->ssdp);
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

int upnp_register_client (upnp_t *upnp, int (*callback) (void *cookie, upnp_event_t *), void *cookie)
{
	pthread_mutex_lock(&upnp->mutex);
	upnp->type.type = UPNP_TYPE_CLIENT;
	upnp->type.client.callback = callback;
	upnp->type.client.cookie = cookie;
	pthread_mutex_unlock(&upnp->mutex);
	return 0;
}

int upnp_register_device (upnp_t *upnp, const char *description, int (*callback) (void *cookie, upnp_event_t *), void *cookie)
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
	char *serviceid;
	char *eventurl;
	char *controlurl;

	char *deviceusn;

	upnp_service_t *service;

	pthread_mutex_lock(&upnp->mutex);
	upnp->type.type = UPNP_TYPE_DEVICE;
	list_init(&upnp->type.device.services);
	upnp->type.device.description = strdup(description);
	upnp->type.device.callback = callback;
	upnp->type.device.cookie = cookie;
	if (upnp->type.device.description == NULL) {
		pthread_mutex_unlock(&upnp->mutex);
		return -1;
	}
	if (asprintf(&upnp->type.device.location, "http://%s:%d/description.xml", upnp->host, upnp->port) < 0) {
		free(upnp->type.device.description);
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
				ssdp_register(upnp->ssdp, "upnp:rootdevice", deviceusn, upnp->type.device.location, "mini upnp stack 1.0", 100000);
				free(deviceusn);
			}
			ssdp_register(upnp->ssdp, deviceudn, deviceudn, upnp->type.device.location, "mini upnp stack 1.0", 100000);
			if (asprintf(&deviceusn, "%s::%s", deviceudn, devicetype) > 0) {
				ssdp_register(upnp->ssdp, devicetype, deviceusn, upnp->type.device.location, "mini upnp stack 1.0", 100000);
				ssdp_register(upnp->ssdp, deviceudn, deviceusn, upnp->type.device.location, "mini upnp stack 1.0", 100000);
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
				serviceid = upnp_ixml_getfirstelementitem((IXML_Element *) servicenode, "serviceId");
				eventurl = upnp_ixml_getfirstelementitem((IXML_Element *) servicenode, "eventSubURL");
				controlurl = upnp_ixml_getfirstelementitem((IXML_Element *) servicenode, "controlURL");
				if (servicetype == NULL || eventurl == NULL || controlurl == NULL || serviceid == NULL) {
					goto __continue;
				}
				if (asprintf(&deviceusn, "%s::%s", deviceudn, servicetype) > 0) {
					if (ssdp_register(upnp->ssdp, servicetype, deviceusn, upnp->type.device.location, "mini upnp stack 1.0", 100000) == 0) {
						service = (upnp_service_t *) malloc(sizeof(upnp_service_t));
						if (service != NULL) {
							memset(service, 0, sizeof(upnp_service_t));
							list_init(&service->head);
							list_init(&service->subscribers);
							service->udn = strdup(deviceudn);
							if (service->udn == NULL) {
								free(service);
							} else {
								service->eventurl = eventurl;
								service->controlurl = controlurl;
								service->serviceid = serviceid;
								debugf("adding service: %s", serviceid);
								list_add(&service->head, &upnp->type.device.services);
								controlurl = NULL;
								eventurl = NULL;
								serviceid = NULL;
							}
						}
					}
					free(deviceusn);
				}
__continue:
				free(controlurl);
				free(eventurl);
				free(serviceid);
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
	upnp->ssdp = ssdp_init(ssdp_callback_event, upnp);
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

	return upnp;
}

int upnp_uninit (upnp_t *upnp)
{
	upnp_service_t *s;
	upnp_service_t *sn;
	upnp_subscribe_t *c;
	upnp_subscribe_t *cn;
	debugf("calling ssdp_uninit");
	ssdp_uninit(upnp->ssdp);
	debugf("calling gena_uninit");
	gena_uninit(upnp->gena);
	debugf("free device memory");
	if (upnp->type.type == UPNP_TYPE_DEVICE) {
		list_for_each_entry_safe(s, sn, &upnp->type.device.services, head) {
			list_for_each_entry_safe(c, cn, &s->subscribers, head) {
				list_del(&c->head);
				free(c->url.host);
				free(c->url.url);
				free(c->url.path);
				free(c);
			}
			list_del(&s->head);
			free(s->udn);
			free(s->eventurl);
			free(s->controlurl);
			free(s->serviceid);
			free(s);
		}
		free(upnp->type.device.description);
		free(upnp->type.device.location);
	}
	pthread_mutex_destroy(&upnp->mutex);
	free(upnp->host);
	free(upnp);
	return 0;
}

IXML_Document * upnp_makeaction (upnp_t *upnp, const char *actionname, const char *controlurl, const char *servicetype, const int param_count, char **param_name, char **param_val)
{
	int p;
	char *data;
	char *result;
	char *buffer;
	upnp_url_t url;
	IXML_Node *text;
	IXML_Node *node;
	IXML_Element *elem;
	IXML_Document *action;
	IXML_Document *response;
	const char *format_post =
		"POST /%s HTTP/1.1\r\n"
		"HOST: %s:%d\r\n"
		"CONTENT-LENGTH: %u\r\n"
		"CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
		"SOAPACTION: \"%s#%s\"\r\n"
		"\r\n"
		"%s%s%s";
	const char *format_envelope_start =
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
		" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
		"<s:Body>\n";
	const char *format_envelope_end =
		"</s:Body>\n"
		"</s:Envelope>\n";
	const char *format_action =
		"<u:%s xmlns:u=\"%s\">\n"
		"</u:%s>\n";
	buffer = NULL;
	action = NULL;
	if (upnp_url_parse(controlurl, &url) != 0) {
		return NULL;
	}
	if (asprintf(&buffer, format_action, actionname, servicetype, actionname) < 0) {
		upnp_url_uninit(&url);
		return NULL;
	}
	if (ixmlParseBufferEx(buffer, &action) != IXML_SUCCESS) {
		upnp_url_uninit(&url);
		free(buffer);
		return NULL;
	}
	free(buffer);
	if (action == NULL) {
		upnp_url_uninit(&url);
		return NULL;
	}
	for (p = 0; p < param_count; p++) {
                node = ixmlNode_getFirstChild((IXML_Node *) action);
                elem = ixmlDocument_createElement(action, param_name[p]);
                if (param_val[p] != NULL) {
                        text = ixmlDocument_createTextNode(action, param_val[p]);
                        ixmlNode_appendChild((IXML_Node *) elem, text);
                }
                ixmlNode_appendChild(node, (IXML_Node *) elem);
	}
	buffer = ixmlPrintNode((IXML_Node *) action);
	ixmlDocument_free(action);
	data = NULL;
	if (asprintf(&data, format_post,
			url.path,
			url.host, url.port,
			strlen(buffer) + strlen(format_envelope_start) + strlen(format_envelope_end),
			servicetype, actionname,
			format_envelope_start, buffer, format_envelope_end) < 0) {
	}
	free(buffer);
	result = gena_send_recv(upnp->gena, url.host, url.port, data, NULL);
	free(data);
	if (result != NULL) {
		if (ixmlParseBufferEx(result, &response) != IXML_SUCCESS) {
			response = NULL;
	        }

	}
	free(result);
	upnp_url_uninit(&url);
	return response;
}

int upnp_search (upnp_t *upnp, int timeout, const char *uuid)
{
	int ret;
	pthread_mutex_lock(&upnp->mutex);
	ret = ssdp_search(upnp->ssdp, uuid, timeout);
	pthread_mutex_unlock(&upnp->mutex);
	return 0;
}

int upnp_subscribe (upnp_t *upnp, const char *serviceurl, int *timeout, char **sid)
{
	return 0;
}

int upnp_resolveurl (const char *baseurl, const char *relativeurl, char *absoluteurl)
{
	upnp_url_t url;
	if (upnp_url_parse(baseurl, &url) != 0) {
		return -1;
	}
	while (*relativeurl == '/') {
		relativeurl++;
	}
	sprintf(absoluteurl, "%s:%d/%s", url.host, url.port, relativeurl);
	upnp_url_uninit(&url);
	return 0;
}

char * upnp_download (upnp_t *upnp, const char *location)
{
	char *buffer;
	upnp_url_t url;
	if (upnp_url_parse(location, &url) != 0) {
		return NULL;
	}
	buffer = gena_download(upnp->gena, url.host, url.port, url.path);
	upnp_url_uninit(&url);
	return buffer;
}
