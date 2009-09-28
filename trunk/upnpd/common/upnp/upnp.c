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
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <signal.h>

#include "platform.h"
#include "xmlparser.h"
#include "ssdp.h"
#include "gena.h"
#include "upnp.h"
#include "uuid.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

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
	thread_mutex_t *mutex;
	gena_callbacks_t gena_callbacks;
	gena_callback_vfs_t *vfscallbacks;
};

static int gena_callback_info (void *cookie, char *path, gena_fileinfo_t *info)
{
	int ret;
	upnp_t *upnp;
	ret = -1;
	upnp = (upnp_t *) cookie;
	thread_mutex_lock(upnp->mutex);
	if (strcmp(path, "/description.xml") == 0) {
		info->size = strlen(upnp->type.device.description);
		info->mtime = 0;
		info->mimetype = strdup("text/xml");
		thread_mutex_unlock(upnp->mutex);
		return 0;
	}
	if (upnp->vfscallbacks != NULL &&
	    upnp->vfscallbacks->info != NULL) {
		ret = upnp->vfscallbacks->info(upnp->vfscallbacks->cookie, path, info);
	}
	thread_mutex_unlock(upnp->mutex);
	return ret;
}

static void * gena_callback_open (void *cookie, char *path, gena_filemode_t mode)
{
	upnp_t *upnp;
	gena_file_t *file;
	upnp = (upnp_t *) cookie;
	file = (gena_file_t *) malloc(sizeof(gena_file_t));
	memset(file, 0, sizeof(gena_file_t));
	thread_mutex_lock(upnp->mutex);
	if (strcmp(path, "/description.xml") == 0) {
		file->virtual = 1;
		file->size = strlen(upnp->type.device.description);
		file->buf = strdup(upnp->type.device.description);
		if (file->buf == NULL) {
			free(file);
			file = NULL;
		}
		thread_mutex_unlock(upnp->mutex);
		return file;
	}
	if (upnp->vfscallbacks != NULL &&
	    upnp->vfscallbacks->open != NULL) {
		file->data = upnp->vfscallbacks->open(upnp->vfscallbacks->cookie, path, mode);
		if (file->data == NULL) {
			free(file);
			file = NULL;
		}
		thread_mutex_unlock(upnp->mutex);
		return file;
	}
	thread_mutex_unlock(upnp->mutex);
	return NULL;
}

static int gena_callback_read (void *cookie, void *handle, char *buffer, unsigned int length)
{
	size_t len;
	upnp_t *upnp;
	gena_file_t *file;
	upnp = (upnp_t *) cookie;
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
	if (upnp->vfscallbacks != NULL &&
	    upnp->vfscallbacks->read != NULL) {
		return upnp->vfscallbacks->read(upnp->vfscallbacks->cookie, file->data, buffer, length);
	}
	return -1;
}

static int gena_callback_write (void *cookie, void *handle, char *buffer, unsigned int length)
{
	return 0;
}

static unsigned long gena_callback_seek (void *cookie, void *handle, long offset, gena_seek_t whence)
{
	upnp_t *upnp;
	gena_file_t *file;
	upnp = (upnp_t *) cookie;
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
	if (upnp->vfscallbacks != NULL &&
	    upnp->vfscallbacks->seek != NULL) {
		return upnp->vfscallbacks->seek(upnp->vfscallbacks->cookie, file->data, offset, whence);
	}
	return -1;
}

static int gena_callback_close (void *cookie, void *handle)
{
	int ret;
	upnp_t *upnp;
	gena_file_t *file;
	upnp = (upnp_t *) cookie;
	file = (gena_file_t *) handle;
	if (file == NULL) {
		return -1;
	}
	if (file->virtual == 1) {
		free(file->buf);
		free(file);
		return 0;
	}
	if (upnp->vfscallbacks != NULL &&
	    upnp->vfscallbacks->close != NULL) {
		ret = upnp->vfscallbacks->close(upnp->vfscallbacks->cookie, file->data);
		free(file);
		return ret;
	}
	return 0;
}

static char * strdup_escaped (const char *p )
{
	int i;
	int j;
	int plen;
	int dlen;
	char *buf;
	if (p == NULL) {
		return NULL;
	}
	buf = NULL;
	dlen = 0;
	plen = strlen(p);
	for (i = 0; i < plen; i++) {
		switch (p[i]) {
			case '<':  dlen += 4; break;
			case '>':  dlen += 4; break;
			case '&':  dlen += 5; break;
			case '\'': dlen += 6; break;
			case '\"': dlen += 6; break;
			default:   dlen += 1; break;
		}
	}
	buf = (char *) malloc(dlen + 1);
	if (buf == NULL) {
		return NULL;
	}
	for (j = 0, i = 0; i < plen; i++) {
		switch (p[i]) {
			case '<':  buf[j++] = '&'; buf[j++] = 'l'; buf[j++] = 't'; buf[j++] = ';'; break;
			case '>':  buf[j++] = '&'; buf[j++] = 'g'; buf[j++] = 't'; buf[j++] = ';'; break;
			case '&':  buf[j++] = '&'; buf[j++] = 'a'; buf[j++] = 'm'; buf[j++] = 'p'; buf[j++] = ';'; break;
			case '\'': buf[j++] = '&'; buf[j++] = 'a'; buf[j++] = 'p'; buf[j++] = 'o'; buf[j++] = 's'; buf[j++] = ';'; break;
			case '\"': buf[j++] = '&'; buf[j++] = 'q'; buf[j++] = 'u'; buf[j++] = 'o'; buf[j++] = 't'; buf[j++] = ';'; break;
			default:   buf[j++] = p[i]; break;
		}
	}
	buf[j] = '\0';
	return buf;
}

int upnp_addtoactionresponse (upnp_event_action_t *response, const char *service, const char *variable, const char *value)
{
	if (service == NULL) {
		return -1;
	}
	if (response->response.service == NULL) {
		response->response.service = strdup(service);
		if (response->response.service == NULL) {
			return -1;
		}
	}
	if (variable != NULL) {
		upnp_event_action_node_t *node;
		node = (upnp_event_action_node_t *) malloc(sizeof(*node));
		if (node == NULL) {
			return -1;
		}
		memset(node, 0, sizeof(*node));
		node->variable = strdup(variable);
		if (node->variable == NULL) {
			free(node);
			return -1;
		}
		if (value != NULL) {
			node->value = strdup_escaped(value);
			if (node->value == NULL) {
				free(node->variable);
				free(node);
				return -1;
			}
		}
		list_add_tail(&node->head, &response->response.nodes);
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
	thread_mutex_lock(upnp->mutex);
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

	debugf("header: %s\n", header);
	debugf("propset: %s\n", propset);

	data = gena_send_recv(upnp->gena, c->url.host, c->url.port, header, propset);
	if (data == NULL) {
		debugf("gena_send_recv() failed");
	}

	free(data);
	free(header);
	free(propset);
out:
	thread_mutex_unlock(upnp->mutex);
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
	memset(new, 0, len + 1);
	memcpy(new, s, len);
	return new;
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
	memset(url, 0, sizeof(upnp_url_t));
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
	uuid_gen_t uuid;
	upnp_service_t *s;
	upnp_subscribe_t *c;
	ret = -1;
	debugf("enter");
	thread_mutex_lock(upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		debugf("eventurl: %s", s->eventurl);
		if (strcmp(subscribe->path, s->eventurl) == 0) {
			c = (upnp_subscribe_t *) malloc(sizeof(upnp_subscribe_t));
			if (c == NULL) {
				ret = -1;
				goto out;
			}
			memset(c, 0, sizeof(upnp_subscribe_t));
			upnp_uuid_generate(&uuid);
			sprintf(c->sid, "uuid:%s", uuid.uuid);
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
out:	thread_mutex_unlock(upnp->mutex);
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
	thread_mutex_lock(upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(subscribe->path, s->eventurl) == 0) {
			memset(&e, 0, sizeof(upnp_event_t));
			e.type = UPNP_EVENT_TYPE_SUBSCRIBE_REQUEST;
			e.event.subscribe.serviceid = s->serviceid;
			e.event.subscribe.udn = s->udn;
			e.event.subscribe.sid = subscribe->sid;
			thread_mutex_unlock(upnp->mutex);
			if (upnp->type.device.callback != NULL) {
				upnp->type.device.callback(upnp->type.device.cookie, &e);
			}
			thread_mutex_lock(upnp->mutex);
			ret = 0;
			goto out;
		}
	}
out:	thread_mutex_unlock(upnp->mutex);
	debugf("ret: %d", ret);
	return ret;
}

static int gena_callback_event_subscribe_renew (upnp_t *upnp, gena_event_subscribe_t *subscribe)
{
	int ret;
	upnp_service_t *s;
	upnp_subscribe_t *c;
	ret = -1;
	thread_mutex_lock(upnp->mutex);
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
out:	thread_mutex_unlock(upnp->mutex);
	return -1;
}

static int gena_callback_event_subscribe_drop (upnp_t *upnp, gena_event_unsubscribe_t *unsubscribe)
{
	upnp_service_t *s;
	upnp_subscribe_t *c;
	upnp_subscribe_t *cn;
	thread_mutex_lock(upnp->mutex);
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
	thread_mutex_unlock(upnp->mutex);
	return 0;
}

static int gena_callback_event_action (upnp_t *upnp, gena_event_action_t *action)
{
	int ret;
	upnp_event_t e;
	upnp_service_t *s;
	char *response;
	char *tmp;
	upnp_event_action_node_t *n;
	upnp_event_action_node_t *n_;
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
	thread_mutex_lock(upnp->mutex);
	list_for_each_entry(s, &upnp->type.device.services, head) {
		if (strcmp(action->path, s->controlurl) == 0) {
			memset(&e, 0, sizeof(upnp_event_t));
			e.type = UPNP_EVENT_TYPE_ACTION;
			e.event.action.action = action->action;
			e.event.action.request = action->request;
			e.event.action.serviceid = s->serviceid;
			e.event.action.udn = s->udn;
			list_init(&e.event.action.response.nodes);
			thread_mutex_unlock(upnp->mutex);
			if (upnp->type.device.callback != NULL) {
				upnp->type.device.callback(upnp->type.device.cookie, &e);
			}
			if (e.event.action.errcode == 0) {
				tmp = NULL;
				if (asprintf(&tmp,
						"<u:%sResponse xmlns:u=\"%s\">\n",
						 e.event.action.action,
						 e.event.action.response.service) < 0) {
					response = NULL;
					tmp = NULL;
				} else {
					response = tmp;
					tmp = NULL;
					list_for_each_entry(n, &e.event.action.response.nodes, head) {
						if (asprintf(&tmp,
								"%s<%s>%s</%s>\n",
								response,
								n->variable,
								n->value,
								n->variable) < 0) {
							free(response);
							response = NULL;
							tmp = NULL;
							break;
						} else {
							free(response);
							response = tmp;
							tmp = NULL;
						}
					}
					if (response != NULL) {
						if (asprintf(&tmp,
								"%s</u:%sResponse>",
								response,
								e.event.action.action) < 0) {
							free(response);
							response = NULL;
							tmp = NULL;
						} else {
							free(response);
							response = tmp;
							tmp = NULL;
						}
					}
				}
				if (response != NULL) {
					if (asprintf(&action->response,
							envelope,
							response) < 0) {
						action->response = NULL;
					}
					free(response);
					response = NULL;
				}
			} else {
				response = NULL;
				if (asprintf(&action->response, fault, upnp_errors[e.event.action.errcode].code, upnp_errors[e.event.action.errcode].str) < 0) {
					action->response = NULL;
				}
			}
			e.event.action.request = NULL;
			free(e.event.action.response.service);
			list_for_each_entry_safe(n, n_, &e.event.action.response.nodes, head) {
				list_del(&n->head);
				free(n->variable);
				free(n->value);
				free(n);
			}
			free(response);
			thread_mutex_lock(upnp->mutex);
			ret = 0;
			goto out;
		}
	}
out:	thread_mutex_unlock(upnp->mutex);
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

static int ssdp_callback_event_notify (upnp_t *upnp, ssdp_event_t *notify)
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

static int ssdp_callback_event_byebye (upnp_t *upnp, ssdp_event_t *byebye)
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
			return ssdp_callback_event_notify(upnp, event);
		case SSDP_EVENT_TYPE_BYEBYE:
			return ssdp_callback_event_byebye(upnp, event);
		default:
			break;
	}
	return -1;
}

int upnp_advertise (upnp_t *upnp)
{
	int rc;
	thread_mutex_lock(upnp->mutex);
	rc = ssdp_advertise(upnp->ssdp);
	thread_mutex_unlock(upnp->mutex);
	return rc;
}

int upnp_register_client (upnp_t *upnp, int (*callback) (void *cookie, upnp_event_t *), void *cookie)
{
	thread_mutex_lock(upnp->mutex);
	upnp->type.type = UPNP_TYPE_CLIENT;
	upnp->type.client.callback = callback;
	upnp->type.client.cookie = cookie;
	thread_mutex_unlock(upnp->mutex);
	return 0;
}

int upnp_register_device (upnp_t *upnp, const char *description, int (*callback) (void *cookie, upnp_event_t *), void *cookie)
{
	xml_node_t *desc;
	xml_node_t *devicenode;
	xml_node_t *servicelist;
	xml_node_t *servicenode;

	char *devicetype;
	char *deviceudn;

	char *servicetype;
	char *serviceid;
	char *eventurl;
	char *controlurl;

	char *deviceusn;

	upnp_service_t *service;

	thread_mutex_lock(upnp->mutex);
	upnp->type.type = UPNP_TYPE_DEVICE;
	list_init(&upnp->type.device.services);
	upnp->type.device.description = strdup(description);
	upnp->type.device.callback = callback;
	upnp->type.device.cookie = cookie;
	if (upnp->type.device.description == NULL) {
		debugf("upnp->type.device.description can not be null");
		thread_mutex_unlock(upnp->mutex);
		return -1;
	}
	if (asprintf(&upnp->type.device.location, "http://%s:%d/description.xml", upnp->host, upnp->port) < 0) {
		free(upnp->type.device.description);
		debugf("upnp->type.device.location can not be null");
		thread_mutex_unlock(upnp->mutex);
		return -1;
	}

	desc = xml_parse_buffer(description, strlen(description));
	if (desc == NULL) {
		debugf("could not parse description:\n'%s'", description);
		thread_mutex_unlock(upnp->mutex);
		return -1;
	}

	list_for_each_entry(devicenode, &desc->nodes, head) {
		if (strcmp(devicenode->name, "device") != 0) {
			continue;
		}
		devicetype = xml_node_get_path_value(devicenode, "deviceType");
		deviceudn = xml_node_get_path_value(devicenode, "UDN");
		if (devicetype == NULL || deviceudn == NULL) {
			continue;
		}

		debugf("registering device to ssdp");
		debugf("  deviceType:'%s'", devicetype);
		debugf("  UDN       :'%s'", deviceudn);

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

		servicelist = xml_node_get_path(devicenode, "serviceList");
		if (servicelist != NULL) {
			list_for_each_entry(servicenode, &servicelist->nodes, head) {
				servicetype = xml_node_get_path_value(servicenode, "serviceType");
				serviceid = strdup(xml_node_get_path_value(servicenode, "serviceId"));
				eventurl = strdup(xml_node_get_path_value(servicenode, "eventSubURL"));
				controlurl = strdup(xml_node_get_path_value(servicenode, "controlURL"));
				if (servicetype == NULL || eventurl == NULL || controlurl == NULL || serviceid == NULL) {
					goto __continue;
				}
				debugf("registering service to ssdp");
				debugf("  serviceType:'%s'", servicetype);
				debugf("  serviceId  :'%s'", serviceid);
				debugf("  eventSubURL:'%s'", eventurl);
				debugf("  controlURL :'%s'", controlurl);
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
			}
		}
	}

	xml_node_uninit(desc);
	thread_mutex_unlock(upnp->mutex);
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

upnp_t * upnp_init (const char *host, const unsigned short port, gena_callback_vfs_t *vfscallbacks, void *vfscookie)
{
	upnp_t *upnp;
	debugf("setting seed");
	rand_srand(time_gettimeofday());
	debugf("ignoring sigpipe signal");
	signal(SIGPIPE, SIG_IGN);
	debugf("initializing upnp stack");
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

	upnp->vfscallbacks = vfscallbacks;
	if (upnp->vfscallbacks != NULL) {
		upnp->vfscallbacks->cookie = vfscookie;
	}

	upnp->gena = gena_init(upnp->host, upnp->port, &upnp->gena_callbacks);
	if (upnp->gena == NULL) {
		ssdp_uninit(upnp->ssdp);
		free(upnp->host);
		free(upnp);
		return NULL;
	}
	upnp->port = gena_getport(upnp->gena);

	upnp->mutex = thread_mutex_init("upnp->mutex", 0);

	return upnp;
}

int upnp_uninit (upnp_t *upnp)
{
	upnp_service_t *s;
	upnp_service_t *sn;
	upnp_subscribe_t *c;
	upnp_subscribe_t *cn;
	if (upnp == NULL) {
		return 0;
	}
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
	thread_mutex_destroy(upnp->mutex);
	free(upnp->host);
	free(upnp);
	return 0;
}

char * upnp_makeaction (upnp_t *upnp, const char *actionname, const char *controlurl, const char *servicetype, const int param_count, char **param_name, char **param_val)
{
	int p;
	char *tmp;
	char *ptr;
	char *str;
	char *data;
	char *result;
	char *buffer;
	upnp_url_t url;
	const char *format_post =
		"POST /%s HTTP/1.1\r\n"
		"HOST: %s:%d\r\n"
		"CONTENT-LENGTH: %u\r\n"
		"CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
		"SOAPACTION: \"%s#%s\"\r\n"
		"\r\n"
		"%s";
	const char *format_envelope_start =
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
		" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
		"<s:Body>\n"
		"<u:%s xmlns:u=\"%s\">\n";
	const char *format_envelope_end =
		"%s</u:%s>\n"
		"</s:Body>\n"
		"</s:Envelope>\n";
	buffer = NULL;
	if (upnp_url_parse(controlurl, &url) != 0) {
		return NULL;
	}
	tmp = NULL;
	buffer = tmp;
	if (asprintf(&tmp, format_envelope_start, actionname, servicetype) < 0) {
		upnp_url_uninit(&url);
		free(buffer);
		return NULL;
	}
	free(buffer);
	buffer = tmp;
	tmp = NULL;
	for (p = 0; p < param_count; p++) {
		ptr = strdup_escaped(param_name[p]);
		str = strdup_escaped(param_val[p]);
		if (asprintf(&tmp, "%s<%s>%s</%s>\n", buffer, ptr, (str != NULL) ? str : "", ptr) < 0) {
			free(ptr);
			free(str);
			upnp_url_uninit(&url);
			free(buffer);
			return NULL;
		}
		free(ptr);
		free(str);
		free(buffer);
		buffer = tmp;
		tmp = NULL;
	}
	if (asprintf(&tmp, format_envelope_end, buffer, actionname) < 0) {
		upnp_url_uninit(&url);
		free(buffer);
		return NULL;
	}
	free(buffer);
	buffer = tmp;
	tmp = NULL;
	p = strlen(buffer);
	if (asprintf(&data, format_post,
			url.path,
			url.host, url.port,
			p,
			servicetype, actionname,
			buffer) < 0) {
	}
	free(buffer);
	debugf("sending action data:\n'%s'\n", data);
	result = gena_send_recv(upnp->gena, url.host, url.port, data, NULL);
	free(data);
	debugf("result: '%s'", result);
	upnp_url_uninit(&url);
	return result;
}

int upnp_search (upnp_t *upnp, int timeout, const char *uuid)
{
	int ret;
	thread_mutex_lock(upnp->mutex);
	ret = ssdp_search(upnp->ssdp, uuid, timeout);
	thread_mutex_unlock(upnp->mutex);
	return 0;
}

int upnp_subscribe (upnp_t *upnp, const char *serviceurl, int *timeout, char **sid)
{
	debugf("upnp subscribe is not supported, yet");
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
