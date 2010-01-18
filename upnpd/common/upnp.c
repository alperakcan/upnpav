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
#include <assert.h>

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

static int strappend (char **to, char *append)
{
	int l;
	int j;
	char *tmp;
	if (*to == NULL) {
		l = 0;
	} else {
		l = strlen(*to);
	}
	if (append == NULL) {
		return 0;
	}
	j = strlen(append);
	if (j == 0) {
		return 0;
	}
	tmp = (char *) malloc(l + j + 1);
	if (tmp == NULL) {
		return -1;
	}
	if (l > 0) {
		memcpy(tmp, *to, l);
	}
	memcpy(tmp + l, append, j + 1);
	free(*to);
	*to = tmp;
	return 0;
}

static int strappend_int (char **to, int i)
{
	int r;
	char *append;
	append = NULL;
	if (asprintf(&append, "%d", i) < 0) {
		return -1;
	}
	r = strappend(to, append);
	free(append);
	return r;
}

#define APPEND(a) if (strappend(result, a) != 0) { goto error; }
#define APPENDI(a) if (strappend_int(result, a) != 0) { goto error; }

static int generate_description (char **result, device_t *device)
{
	int i;
	icon_t *icon;
	device_service_t *service;
	APPEND("\n<root xmlns=\"urn:schemas-upnp-org:device-1-0\">");
	APPEND("\n<specVersion>\n<major>1</major>\n<minor>0</minor>\n</specVersion>");
	APPEND("\n<device>");
	APPEND("\n<deviceType>"); APPEND(device->devicetype); APPEND("</deviceType>");
	APPEND("\n<presentationURL>"); APPEND(device->presentationurl); APPEND("</presentationURL>");
	APPEND("\n<friendlyName>"); APPEND(device->friendlyname); APPEND("</friendlyName>");
	APPEND("\n<manufacturer>"); APPEND(device->manufacturer); APPEND("</manufacturer>");
	APPEND("\n<manufacturerURL>"); APPEND(device->manufacturerurl); APPEND("</manufacturerURL>");
	APPEND("\n<modelDescription>"); APPEND(device->modeldescription); APPEND("</modelDescription>");
	APPEND("\n<modelName>"); APPEND(device->modelname); APPEND("</modelName>");
	APPEND("\n<modelURL>"); APPEND(device->modelurl); APPEND("</modelURL>");
	APPEND("\n<UDN>"); APPEND(device->uuid); APPEND("</UDN>");
	APPEND("\n<dlna:X_DLNADOC xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">DMS-1.50</dlna:X_DLNADOC>");
	if (device->icons != NULL) {
		APPEND("\n<iconList>");
		for (i = 0; (icon = device->icons[i]) != NULL; i++) {
			APPEND("\n<icon>");
			APPEND("\n<mimetype>"); APPEND(icon->mimetype); APPEND("</mimetype>");
			APPEND("\n<width>"); APPENDI(icon->width); APPEND("</width>");
			APPEND("\n<height>"); APPENDI(icon->height); APPEND("</height>");
			APPEND("\n<depth>"); APPENDI(icon->depth); APPEND("</depth>");
			APPEND("\n<url>"); APPEND(icon->url); APPEND("</url>");
			APPEND("\n</icon>");
		}
		APPEND("\n</iconList>");
	}
	if (device->services) {
		APPEND("\n<serviceList>");
		for (i = 0; (service = device->services[i]) != NULL; i++) {
			APPEND("\n<service>");
			APPEND("\n<serviceId>"); APPEND(service->id); APPEND("</serviceId>");
			APPEND("\n<serviceType>"); APPEND(service->type); APPEND("</serviceType>");
			APPEND("\n<SCPDURL>"); APPEND(service->scpdurl); APPEND("</SCPDURL>");
			APPEND("\n<controlURL>"); APPEND(service->controlurl); APPEND("</controlURL>");
			APPEND("\n<eventSubURL>"); APPEND(service->eventurl); APPEND("</eventSubURL>");
			APPEND("\n</service>");
		}
		APPEND("\n</serviceList>");
	}
	APPEND("\n</device>\n</root>\n");
	return 0;
error:
	return -1;
}

static int generate_scpd (char **result, device_service_t *service)
{
	int i;
	int j;
	char *datatype;
	char *allowedvalue;
	service_action_t *action;
	action_argument_t *argument;
	service_variable_t *variable;

	APPEND("\n<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">");
	APPEND("\n<specVersion>\n<major>1</major>\n<minor>0</minor>\n</specVersion>");
	if (service->actions != NULL) {
		APPEND("\n<actionList>");
		for (i = 0; (action = service->actions[i]) != NULL; i++) {
			APPEND("\n<action>");
			APPEND("\n<name>"); APPEND(action->name); APPEND("</name>");
			if (action->arguments != NULL) {
				APPEND("\n<argumentList>");
				for (j = 0; (argument = action->arguments[j]) != NULL; j++) {
					APPEND("\n<argument>");
					APPEND("\n<name>"); APPEND(argument->name); APPEND("</name>");
					if (argument->direction == ARGUMENT_DIRECTION_IN) {
						APPEND("\n<direction>in</direction>");
					} else {
						APPEND("\n<direction>out</direction>");
					}
					APPEND("\n<relatedStateVariable>"); APPEND(argument->relatedstatevariable); APPEND("</relatedStateVariable>");
					APPEND("\n</argument>");
				}
				APPEND("\n</argumentList>");
			}
			APPEND("\n</action>");
		}
		APPEND("\n</actionList>");
	}
	if (service->variables != NULL) {
		APPEND("\n<serviceStateTable>");
		for (i = 0; (variable = service->variables[i]) != NULL; i++) {
			datatype = NULL;
			switch (variable->datatype) {
				case VARIABLE_DATATYPE_STRING:	datatype = "string"; break;
				case VARIABLE_DATATYPE_I4:	datatype = "i4"; break;
				case VARIABLE_DATATYPE_UI4:	datatype = "ui4"; break;
				case VARIABLE_DATATYPE_BIN_BASE64: datatype = "bin.base64"; break;
				default: assert(0); break;
			}
			if (variable->sendevent == VARIABLE_SENDEVENT_YES) {
				APPEND("\n<stateVariable sendEvents=\"yes\">");
			} else {
				APPEND("\n<stateVariable sendEvents=\"no\">");
			}
			APPEND("\n<name>"); APPEND(variable->name); APPEND("</name>");
			APPEND("\n<dataType>"); APPEND(datatype); APPEND("</dataType>");
			if (variable->allowedvalues != NULL) {
				APPEND("\n<allowedValueList>");
				for (j = 0; (allowedvalue = variable->allowedvalues[j]) != NULL; j++) {
					APPEND("\n<allowedValue>"); APPEND(allowedvalue); APPEND("</allowedValue>");
				}
				APPEND("\n</allowedValueList>");
			}
			if (variable->defaultvalue) {
				APPEND("\n<defaultValue>"); APPEND(variable->defaultvalue); APPEND("</defaultValue>");
			}
			APPEND("\n</stateVariable>");
		}
		APPEND("\n</serviceStateTable>");
	}
	APPEND("\n</scpd>\n");
	return 0;
error:
	return -1;
}

char * description_generate_from_service (device_service_t *service)
{
	char *ret = NULL;
	if (strappend(&ret, "<?xml version=\"1.0\"?>") != 0) {
		goto error;
	}
	if (generate_scpd(&ret, service) != 0) {
		goto error;
	}
	return ret;
error:
	debugf("generate_scpd(service) failed");
	free(ret);
	return NULL;
}

char * description_generate_from_device (device_t *device)
{
	char *ret = NULL;
	if (strappend(&ret, "<?xml version=\"1.0\"?>") != 0) {
		goto error;
	}
	if (generate_description(&ret, device) != 0) {
		goto error;
	}
	return ret;
error:
	debugf("generate_description(device) failed");
	free(ret);
	return NULL;
}

int upnp_add_response (upnp_event_action_t *request, char *servicetype, char *key, const char *value)
{
	int rc;
	debugf("adding '%s'='%s' to response", key, value);
	rc = upnp_addtoactionresponse(request, servicetype, key, value);
	if (rc != 0) {
		debugf("upnp_addtoactionresponse() failed");
		request->errcode = UPNP_ERROR_ACTION_FAILED;
		return -1;
	}
	return 0;
}
