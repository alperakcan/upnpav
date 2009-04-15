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

#if defined(ENABLE_CONTROLLER)
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>
#endif

#include "upnp.h"
#include "common.h"

static void add_value_attribute (IXML_Document *doc, IXML_Element *parent, char *attrname, char *value)
{
	ixmlElement_setAttribute(parent, attrname, value);
}

static void add_value_element (IXML_Document *doc, IXML_Element *parent, char *tagname, char *value)
{
	IXML_Element *top;
	IXML_Node *child;
	top = ixmlDocument_createElement(doc, tagname);
	child = ixmlDocument_createTextNode(doc, value);
	ixmlNode_appendChild((IXML_Node *) top, (IXML_Node *) child);
	ixmlNode_appendChild((IXML_Node *) parent, (IXML_Node *) top);
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
		if (asprintf(&scdpurl, "http://%s:%u%s", webserver_getaddress(service->device->webserver), webserver_getport(service->device->webserver), service->scpdurl) < 0) {
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

static IXML_Element * generate_scpd_action (service_action_t *action, IXML_Document *doc)
{
	int j;
	IXML_Element *top;
	IXML_Element *parent;
	IXML_Element *child;
	action_argument_t *argument;

	top = ixmlDocument_createElement(doc, "action");

	add_value_element(doc, top, "name", action->name);
	if (action->arguments) {
		parent=ixmlDocument_createElement(doc, "argumentList");
		ixmlNode_appendChild((IXML_Node *) top,(IXML_Node *) parent);
		for (j = 0; (argument = action->arguments[j]) != NULL; j++) {
			child = ixmlDocument_createElement(doc, "argument");
			ixmlNode_appendChild((IXML_Node *) parent, (IXML_Node *) child);
			add_value_element(doc, child, "name", argument->name);
			add_value_element(doc, child, "direction", (argument->direction == ARGUMENT_DIRECTION_IN) ? "in" : "out");
			add_value_element(doc, child, "relatedStateVariable", argument->relatedstatevariable);
		}
	}
	return top;
}

static IXML_Element * generate_scpd_actionlist (device_service_t *service, IXML_Document *doc)
{
	int i;
	IXML_Element *top;
	IXML_Element *child;
	service_action_t *action;

	top = ixmlDocument_createElement(doc, "actionList");
	for (i = 0; (action = service->actions[i]) != NULL; i++) {
		child = generate_scpd_action(action, doc);
		ixmlNode_appendChild((IXML_Node *) top, (IXML_Node *) child);
	}
	return top;
}

static IXML_Element * generate_scpd_statevar (service_variable_t *variable, IXML_Document *doc)
{
	int i;
	char *datatype;
	char *allowedvalue;

	IXML_Element *top;
	IXML_Element *parent;

	datatype = NULL;

	switch (variable->datatype) {
		case VARIABLE_DATATYPE_STRING:	datatype = "string"; break;
		case VARIABLE_DATATYPE_I4:	datatype = "i4"; break;
		case VARIABLE_DATATYPE_UI4:	datatype = "ui4"; break;
		default: assert(0); break;
	}

	top = ixmlDocument_createElement(doc, "stateVariable");

	add_value_attribute(doc, top, "sendEvents", (variable->sendevent == VARIABLE_SENDEVENT_YES) ? "yes" : "no");
	add_value_element(doc, top, "name", variable->name);
	add_value_element(doc, top, "dataType", datatype);

	if (variable->allowedvalues != NULL) {
		parent = ixmlDocument_createElement(doc, "allowedValueList");
		ixmlNode_appendChild((IXML_Node *) top, (IXML_Node *) parent);
		for (i = 0; (allowedvalue = variable->allowedvalues[i]) != NULL; i++) {
			add_value_element(doc,parent,"allowedValue", allowedvalue);
		}
	}
	if (variable->defaultvalue) {
		add_value_element(doc, top, "defaultValue", variable->defaultvalue);
	}
	return top;
}

static IXML_Element * generate_scpd_servicestatetable (device_service_t *service, IXML_Document *doc)
{
	int i;
	IXML_Element *top;
	IXML_Element *child;
	service_variable_t *variable;

	top = ixmlDocument_createElement(doc, "serviceStateTable");
	for (i = 0; (variable = service->variables[i]) != NULL; i++) {
		child = generate_scpd_statevar(variable, doc);
		ixmlNode_appendChild((IXML_Node *) top, (IXML_Node *) child);
	}
	return top;
}

static IXML_Document * generate_scpd (device_service_t *service)
{
	IXML_Document *doc;
	IXML_Element *root;
	IXML_Element *child;

	doc = ixmlDocument_createDocument();

	root = ixmlDocument_createElementNS(doc, "urn:schemas-upnp-org:service-1-0", "scpd");
	ixmlElement_setAttribute(root, "xmlns", "urn:schemas-upnp-org:service-1-0");
	ixmlNode_appendChild((IXML_Node *) doc, (IXML_Node *) root);

	child = generate_specversion(doc, 1, 0);
	ixmlNode_appendChild((IXML_Node *) root, (IXML_Node *) child);

	child = generate_scpd_actionlist(service, doc);
	ixmlNode_appendChild((IXML_Node *) root, (IXML_Node *) child);

	child = generate_scpd_servicestatetable(service, doc);
	ixmlNode_appendChild((IXML_Node *) root, (IXML_Node *) child);

	return doc;
}

char * description_generate_from_service (device_service_t *service)
{
	char *result = NULL;
	IXML_Document *doc;

	doc = generate_scpd(service);
	if (doc != NULL) {
       		result = ixmlDocumenttoString(doc);
		ixmlDocument_free(doc);
	} else {
		debugf("generate_scdp(service) failed");
	}
	return result;
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
