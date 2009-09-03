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

#ifndef UPNP_H_
#define UPNP_H_

#include <ixml.h>

typedef struct upnp_s upnp_t;

typedef enum {
	UPNP_ERROR_INVALID_ACTION,
	UPNP_ERROR_INVALIG_ARGS,
	UPNP_ERROR_INVALID_VAR,
	UPNP_ERROR_ACTION_FAILED,
	UPNP_ERROR_NOSUCH_OBJECT,
	UPNP_ERROR_INVALID_CURRENTTAG,
	UPNP_ERROR_INVALID_NEWTAG,
	UPNP_ERROR_REQUIRED_TAG,
	UPNP_ERROR_READONLY_TAG,
	UPNP_ERROR_PARAMETER_MISMATCH,
	UPNP_ERROR_INVALID_SEARCH_CRITERIA,
	UPNP_ERROR_INVALID_SORT_CRITERIA,
	UPNP_ERROR_NOSUCH_CONTAINER,
	UPNP_ERROR_RESTRICTED_OBJECT,
	UPNP_ERROR_BAD_METADATA,
	UPNP_ERROR_RESTRICTED_PARENT_OBJECT,
	UPNP_ERROR_NOSUCH_RESOURCE,
	UPNP_ERROR_RESOURCE_ACCESS_DENIED,
	UPNP_ERROR_TRANSFER_BUSY,
	UPNP_ERROR_NOSUCH_TRANSFER,
	UPNP_ERROR_NOSUCH_DESTRESOURCE,
	UPNP_ERROR_DESTRESOURCE_ACCESS_DENIED,
	UPNP_ERROR_CANNOT_PROCESS,
	UPNP_ERROR_INVALID_INSTANCEID,
} upnp_error_type_t;

typedef enum {
	UPNP_EVENT_TYPE_UNKNOWN              = 0x00,
	UPNP_EVENT_TYPE_SUBSCRIBE_REQUEST    = 0x01,
	UPNP_EVENT_TYPE_ACTION               = 0x02,
	UPNP_EVENT_TYPE_ADVERTISEMENT_ALIVE  = 0x03,
	UPNP_EVENT_TYPE_ADVERTISEMENT_BYEBYE = 0x04,
} upnp_event_type_t;

typedef struct upnp_event_subscribe_s {
	char *udn;
	char *serviceid;
	char *sid;
} upnp_event_subscribe_t;

typedef struct upnp_event_action_s {
	char *udn;
	char *serviceid;
	char *action;
	IXML_Document *request;
	int errcode;
	IXML_Document *response;
} upnp_event_action_t;

typedef struct upnp_event_advertisement_s {
	char *device;
	char *location;
	char *uuid;
	int expires;
} upnp_event_advertisement_t;

typedef struct upnp_event_s {
	upnp_event_type_t type;
	union {
		upnp_event_subscribe_t subscribe;
		upnp_event_action_t action;
		upnp_event_advertisement_t advertisement;
	} event;
} upnp_event_t;

typedef struct upnp_url_s {
	char *url;
	char *host;
	unsigned short port;
	char *path;
} upnp_url_t;

#define debugf(fmt...) debug_debugf(__FILE__, __LINE__, __FUNCTION__, fmt);
void debug_debugf (char *file, int line, const char *func, char *fmt, ...);

int upnp_url_uninit (upnp_url_t *url);
int upnp_url_parse (const char *uri, upnp_url_t *url);

char * upnp_download (upnp_t *upnp, const char *location);
IXML_Document * upnp_makeaction (upnp_t *upnp, const char *actionname, const char *controlurl, const char *servicetype, const int param_count, char **param_name, char **param_val);
int upnp_search (upnp_t *upnp, int timeout, const char *uuid);
int upnp_subscribe (upnp_t *upnp, const char *serviceurl, int *timeout, char **sid);
int upnp_resolveurl (const char *baseurl, const char *relativeurl, char *absoluteurl);
int upnp_register_client (upnp_t *upnp, int (*callback) (void *cookie, upnp_event_t *), void *cookie);

int upnp_advertise (upnp_t *upnp);
int upnp_register_device (upnp_t *upnp, const char *description, int (*callback) (void *cookie, upnp_event_t *), void *cookie);
char * upnp_getaddress (upnp_t *upnp);
unsigned short upnp_getport (upnp_t *upnp);
upnp_t * upnp_init (const char *host, const unsigned short port, gena_callback_vfs_t *vfscallbacks, void *vfscookie);
int upnp_uninit (upnp_t *upnp);
int upnp_accept_subscription (upnp_t *upnp, const char *udn, const char *serviceid, const char **variable_names, const char **variable_values, const unsigned int variables_count, const char *sid);
int upnp_addtoactionresponse (upnp_event_action_t *response, const char *service, const char *variable, const char *value);

#endif /* UPNP_H_ */
