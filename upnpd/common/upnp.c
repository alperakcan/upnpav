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
#include <inttypes.h>

#include "platform.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

static void add_value_attribute (IXML_Document *doc, IXML_Element *parent, char *attrname, char *value)
{
	ixmlElement_setAttribute(parent, attrname, value);
}

static IXML_Element * add_value_element (IXML_Document *doc, IXML_Element *parent, char *tagname, char *value)
{
	IXML_Element *top;
	IXML_Node *child;
	top = ixmlDocument_createElement(doc, tagname);
	child = ixmlDocument_createTextNode(doc, value);
	ixmlNode_appendChild((IXML_Node *) top, (IXML_Node *) child);
	ixmlNode_appendChild((IXML_Node *) parent, (IXML_Node *) top);
	return top;
}

static void add_value_element_int (IXML_Document *doc, IXML_Element *parent, char *tagname, int value)
{
	char *buf;
	asprintf(&buf, "%d", value);
	add_value_element(doc, parent, tagname, buf);
	free(buf);
}

static IXML_Element * generate_description_servicelist (IXML_Document *doc, device_service_t **services)
{
	int i;
	char *scdpurl;
	IXML_Element *top;
	IXML_Element *parent;
	device_service_t *service;

	top = ixmlDocument_createElement(doc, "serviceList");
	for (i = 0; (service = services[i]) != NULL; i++) {
		if (asprintf(&scdpurl, "%s", service->scpdurl) < 0) {
			continue;
		}
		parent = ixmlDocument_createElement(doc, "service");
		ixmlNode_appendChild((IXML_Node *) top, (IXML_Node *) parent);
		add_value_element(doc, parent, "serviceId", service->id);
		add_value_element(doc, parent, "serviceType", service->type);
		add_value_element(doc, parent, "SCPDURL", scdpurl);
		add_value_element(doc, parent, "controlURL", service->controlurl);
		add_value_element(doc, parent, "eventSubURL", service->eventurl);
		free(scdpurl);
        }

	return top;
}

static IXML_Element * generate_description_iconlist (IXML_Document *doc, icon_t **icons)
{
	int i;
	icon_t *icon;
	IXML_Element *top;
	IXML_Element *parent;

	top = ixmlDocument_createElement(doc, "iconList");
	for (i = 0; (icon = icons[i]) != NULL; i++) {
		parent = ixmlDocument_createElement(doc, "icon");
		ixmlNode_appendChild((IXML_Node *) top,(IXML_Node *) parent);
		add_value_element(doc, parent, "mimetype", icon->mimetype);
		add_value_element_int(doc, parent, "width", icon->width);
		add_value_element_int(doc, parent, "height", icon->height);
		add_value_element_int(doc, parent, "depth", icon->depth);
		add_value_element(doc, parent, "url", icon->url);
	}

	return top;
}

static IXML_Element * generate_specversion (IXML_Document *doc, int major, int minor)
{
	IXML_Element *top;
	top = ixmlDocument_createElement(doc, "specVersion");
	add_value_element_int(doc, top, "major", major);
	add_value_element_int(doc, top, "minor", minor);
	return top;
}

static IXML_Document * generate_description (device_t *device)
{
	IXML_Document *doc;
	IXML_Element *root;
	IXML_Element *child;
	IXML_Element *parent;

	doc = ixmlDocument_createDocument();

	root = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:device-1-0", "root");
	ixmlElement_setAttribute(root, "xmlns", "urn:schemas-upnp-org:device-1-0");
	ixmlNode_appendChild((IXML_Node *) doc, (IXML_Node *) root);

	child = generate_specversion(doc,1,0);
	ixmlNode_appendChild((IXML_Node *) root, (IXML_Node *) child);

	parent = ixmlDocument_createElement(doc, "device");
	ixmlNode_appendChild((IXML_Node *) root, (IXML_Node *) parent);

	add_value_element(doc, parent, "deviceType", device->devicetype);
	add_value_element(doc, parent, "presentationURL", device->presentationurl);
	add_value_element(doc, parent, "friendlyName", device->friendlyname);
	add_value_element(doc, parent, "manufacturer", device->manufacturer);
	add_value_element(doc, parent, "manufacturerURL", device->manufacturerurl);
	add_value_element(doc, parent, "modelDescription", device->modeldescription);
	add_value_element(doc, parent, "modelName", device->modelname);
	add_value_element(doc, parent, "modelURL", device->modelurl);
	add_value_element(doc, parent, "UDN", device->uuid);
	child = add_value_element(doc, parent, "dlna:X_DLNADOC", "DMS-1.50");
	add_value_attribute(doc, child, "xmlns:dlna", "urn:schemas-dlna-org:device-1-0");

	if (device->icons) {
		child = generate_description_iconlist(doc, device->icons);
		ixmlNode_appendChild((IXML_Node *) parent,(IXML_Node *) child);
	}
	if (device->services) {
		child = generate_description_servicelist(doc, device->services);
		ixmlNode_appendChild((IXML_Node *) parent,(IXML_Node *) child);
	}

	return doc;
}

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

static int generate_scpd (char **result, device_service_t *service)
{
	int i;
	int j;
	char *datatype;
	char *allowedvalue;
	service_action_t *action;
	action_argument_t *argument;
	service_variable_t *variable;

	if (strappend(result, "\n<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">") != 0) { goto error; }
	if (strappend(result, "\n<specVersion>\n<major>1</major>\n<minor>0</minor>\n</specVersion>") != 0) { goto error; }
	if (service->actions != NULL) {
		if (strappend(result, "\n<actionList>") != 0) { goto error; }
		for (i = 0; (action = service->actions[i]) != NULL; i++) {
			if (strappend(result, "\n<action>") != 0) { goto error; }
			if (strappend(result, "\n<name>") != 0) { goto error; }
			if (strappend(result, action->name) != 0) { goto error; }
			if (strappend(result, "</name>") != 0) { goto error; }
			if (action->arguments != NULL) {
				if (strappend(result, "\n<argumentList>") != 0) { goto error; }
				for (j = 0; (argument = action->arguments[j]) != NULL; j++) {
					if (strappend(result, "\n<argument>") != 0) { goto error; }
					if (strappend(result, "\n<name>") != 0) { goto error; }
					if (strappend(result, argument->name) != 0) { goto error; }
					if (strappend(result, "</name>") != 0) { goto error; }
					if (argument->direction == ARGUMENT_DIRECTION_IN) {
						if (strappend(result, "\n<direction>in</direction>") != 0) { goto error; }
					} else {
						if (strappend(result, "\n<direction>out</direction>") != 0) { goto error; }
					}
					if (strappend(result, "\n<relatedStateVariable>") != 0) { goto error; }
					if (strappend(result, argument->relatedstatevariable) != 0) { goto error; }
					if (strappend(result, "</relatedStateVariable>") != 0) { goto error; }
					if (strappend(result, "\n</argument>") != 0) { goto error; }
				}
				if (strappend(result, "\n</argumentList>") != 0) { goto error; }
			}
			if (strappend(result, "\n</action>") != 0) { goto error; }
		}
		if (strappend(result, "\n</actionList>") != 0) { goto error; }
	}
	if (service->variables != NULL) {
		if (strappend(result, "\n<serviceStateTable>") != 0) { goto error; }
		for (i = 0; (variable = service->variables[i]) != NULL; i++) {
			datatype = NULL;
			switch (variable->datatype) {
				case VARIABLE_DATATYPE_STRING:	datatype = "string"; break;
				case VARIABLE_DATATYPE_I4:	datatype = "i4"; break;
				case VARIABLE_DATATYPE_UI4:	datatype = "ui4"; break;
				default: assert(0); break;
			}
			if (variable->sendevent == VARIABLE_SENDEVENT_YES) {
				if (strappend(result, "\n<stateVariable sendEvents=\"yes\">") != 0) { goto error; }
			} else {
				if (strappend(result, "\n<stateVariable sendEvents=\"no\">") != 0) { goto error; }
			}
			if (strappend(result, "\n<name>") != 0) { goto error; }
			if (strappend(result, variable->name) != 0) { goto error; }
			if (strappend(result, "</name>") != 0) { goto error; }
			if (strappend(result, "\n<dataType>") != 0) { goto error; }
			if (strappend(result, datatype) != 0) { goto error; }
			if (strappend(result, "</dataType>") != 0) { goto error; }
			if (variable->allowedvalues != NULL) {
				if (strappend(result, "\n<allowedValueList>") != 0) { goto error; }
				for (j = 0; (allowedvalue = variable->allowedvalues[j]) != NULL; j++) {
					if (strappend(result, "\n<allowedValue>") != 0) { goto error; }
					if (strappend(result, allowedvalue) != 0) { goto error; }
					if (strappend(result, "</allowedValue>") != 0) { goto error; }
				}
				if (strappend(result, "\n</allowedValueList>") != 0) { goto error; }
			}
			if (variable->defaultvalue) {
				if (strappend(result, "\n<defaultValue>") != 0) { goto error; }
				if (strappend(result, variable->defaultvalue) != 0) { goto error; }
				if (strappend(result, "</defaultValue>") != 0) { goto error; }
			}
			if (strappend(result, "\n</stateVariable>") != 0) { goto error; }
		}
		if (strappend(result, "\n</serviceStateTable>") != 0) { goto error; }
	}
	if (strappend(result, "\n</scpd>\n") != 0) { goto error; }
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
	free(ret);
	return NULL;
}

char * description_generate_from_device (device_t *device)
{
	char *result;
	IXML_Document *doc;

	result = NULL;
	doc = generate_description(device);
	if (doc != NULL) {
		result = ixmlDocumenttoString(doc);
		ixmlDocument_free(doc);
	} else {
		debugf("generate_description(device) failed");
	}
	return result;
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
