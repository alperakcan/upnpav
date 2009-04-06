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

void upnp_print_event_type (Upnp_EventType e)
{
	switch (e) {
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
			printf("UPNP_DISCOVERY_ADVERTISEMENT_ALIVE\n");
			break;
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
			printf("UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE\n");
			break;
		case UPNP_DISCOVERY_SEARCH_RESULT:
			printf("UPNP_DISCOVERY_SEARCH_RESULT\n");
			break;
		case UPNP_DISCOVERY_SEARCH_TIMEOUT:
			printf("UPNP_DISCOVERY_SEARCH_TIMEOUT\n");
			break;
	        case UPNP_CONTROL_ACTION_REQUEST:
	    		printf("UPNP_CONTROL_ACTION_REQUEST\n");
	    		break;
	    	case UPNP_CONTROL_ACTION_COMPLETE:
	    		printf("UPNP_CONTROL_ACTION_COMPLETE\n");
	    		break;
	    	case UPNP_CONTROL_GET_VAR_REQUEST:
	    		printf("UPNP_CONTROL_GET_VAR_REQUEST\n");
	    		break;
	    	case UPNP_CONTROL_GET_VAR_COMPLETE:
	    		printf("UPNP_CONTROL_GET_VAR_COMPLETE\n");
	    		break;
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
			printf("UPNP_EVENT_SUBSCRIPTION_REQUEST\n");
			break;
		case UPNP_EVENT_RECEIVED:
			printf("UPNP_EVENT_RECEIVED\n");
			break;
		case UPNP_EVENT_RENEWAL_COMPLETE:
			printf("UPNP_EVENT_RENEWAL_COMPLETE\n");
			break;
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
			printf("UPNP_EVENT_SUBSCRIBE_COMPLETE\n");
			break;
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
			printf("UPNP_EVENT_UNSUBSCRIBE_COMPLETE\n");
			break;
		case UPNP_EVENT_AUTORENEWAL_FAILED:
			printf("UPNP_EVENT_AUTORENEWAL_FAILED\n");
			break;
		case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
			printf("UPNP_EVENT_SUBSCRIPTION_EXPIRED\n");
			break;
	}
}

int upnp_print_event (Upnp_EventType eventtype, void *event)
{
        char *xmlbuff;
	struct Upnp_Event *e_event;
	struct Upnp_Discovery *d_event;
        struct Upnp_Action_Request *ar_event;
	struct Upnp_Action_Complete *ac_event;
	struct Upnp_Event_Subscribe *es_event;
        struct Upnp_State_Var_Request *svr_event;
	struct Upnp_State_Var_Complete *svc_event;
	struct Upnp_Subscription_Request *sr_event;

	upnp_print_event_type(eventtype);

        switch (eventtype) {
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		case UPNP_DISCOVERY_SEARCH_RESULT:
			d_event = (struct Upnp_Discovery *) event;
			printf("ErrCode     =  %d\n", d_event->ErrCode);
			printf("Expires     =  %d\n", d_event->Expires);
			printf("DeviceId    =  %s\n", d_event->DeviceId);
			printf("DeviceType  =  %s\n", d_event->DeviceType);
			printf("ServiceType =  %s\n", d_event->ServiceType);
			printf("ServiceVer  =  %s\n", d_event->ServiceVer);
			printf("Location    =  %s\n", d_event->Location);
			printf("OS          =  %s\n", d_event->Os);
			printf("Ext         =  %s\n", d_event->Ext);
			break;
	        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
			break;
		case UPNP_CONTROL_ACTION_REQUEST:
			xmlbuff = NULL;
			ar_event = (struct Upnp_Action_Request *) event;
			printf("ErrCode     =  %d\n", ar_event->ErrCode);
			printf("ErrStr      =  %s\n", ar_event->ErrStr);
			printf("ActionName  =  %s\n", ar_event->ActionName);
			printf("UDN         =  %s\n", ar_event->DevUDN);
			printf("ServiceID   =  %s\n", ar_event->ServiceID);
			if (ar_event->ActionRequest) {
				xmlbuff = ixmlPrintNode((IXML_Node *) ar_event->ActionRequest);
				if (xmlbuff) {
					printf("ActRequest  =  %s\n", xmlbuff);
					ixmlFreeDOMString(xmlbuff);
					xmlbuff = NULL;
				}
			} else {
				printf("ActRequest  =  (null)\n");
			}
			if (ar_event->ActionResult) {
				xmlbuff = ixmlPrintNode((IXML_Node *) ar_event->ActionResult);
				if (xmlbuff) {
					printf("ActResult   =  %s\n", xmlbuff);
					ixmlFreeDOMString(xmlbuff);
					xmlbuff = NULL;
				}
			} else {
				printf("ActResult   =  (null)\n");
			}
			break;
		case UPNP_CONTROL_ACTION_COMPLETE:
			xmlbuff = NULL;
			ac_event = (struct Upnp_Action_Complete *) event;
			printf("ErrCode     =  %d\n", ac_event->ErrCode);
			printf("CtrlUrl     =  %s\n", ac_event->CtrlUrl);
			if (ac_event->ActionRequest) {
				xmlbuff = ixmlPrintNode((IXML_Node *) ac_event->ActionRequest);
				if (xmlbuff) {
					printf("ActRequest  =  %s\n", xmlbuff);
					ixmlFreeDOMString(xmlbuff);
					xmlbuff = NULL;
				}
			} else {
				printf("ActRequest  =  (null)\n");
			}
			if (ac_event->ActionResult) {
				xmlbuff = ixmlPrintNode((IXML_Node *) ac_event->ActionResult);
				if (xmlbuff) {
					printf("ActResult   =  %s\n", xmlbuff);
					ixmlFreeDOMString(xmlbuff);
					xmlbuff = NULL;
				}
			} else {
				printf("ActResult   =  (null)\n");
			}
			break;
		case UPNP_CONTROL_GET_VAR_REQUEST:
			svr_event = (struct Upnp_State_Var_Request *) event;
			printf("ErrCode     =  %d\n", svr_event->ErrCode);
			printf("ErrStr      =  %s\n", svr_event->ErrStr);
			printf("UDN         =  %s\n", svr_event->DevUDN);
			printf("ServiceID   =  %s\n", svr_event->ServiceID);
			printf("StateVarName=  %s\n", svr_event->StateVarName);
			printf("CurrentVal  =  %s\n", svr_event->CurrentVal);
			break;
		case UPNP_CONTROL_GET_VAR_COMPLETE:
			svc_event = (struct Upnp_State_Var_Complete *) event;
			printf("ErrCode     =  %d\n", svc_event->ErrCode);
			printf("CtrlUrl     =  %s\n", svc_event->CtrlUrl);
			printf("StateVarName=  %s\n", svc_event->StateVarName);
			printf("CurrentVal  =  %s\n", svc_event->CurrentVal);
			break;
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
			sr_event = (struct Upnp_Subscription_Request *) event;
			printf("ServiceID   =  %s\n", sr_event->ServiceId);
			printf("UDN         =  %s\n", sr_event->UDN);
			printf("SID         =  %s\n", sr_event->Sid);
			break;
		case UPNP_EVENT_RECEIVED:
			xmlbuff = NULL;
			e_event = (struct Upnp_Event *) event;
			printf("SID         =  %s\n", e_event->Sid);
			printf("EventKey    =  %d\n", e_event->EventKey);
			xmlbuff = ixmlPrintNode((IXML_Node *) e_event->ChangedVariables);
			if (xmlbuff) {
				printf("ChangedVars =  %s\n", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
				xmlbuff = NULL;
			}
	                break;
	        case UPNP_EVENT_RENEWAL_COMPLETE:
			es_event = (struct Upnp_Event_Subscribe *) event;
			printf("SID         =  %s\n", es_event->Sid);
			printf("ErrCode     =  %d\n", es_event->ErrCode);
			printf("TimeOut     =  %d\n", es_event->TimeOut);
			break;
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
			es_event = (struct Upnp_Event_Subscribe *) event;
			printf("SID         =  %s\n", es_event->Sid);
			printf("ErrCode     =  %d\n", es_event->ErrCode);
			printf("PublisherURL=  %s\n", es_event->PublisherUrl);
			printf("TimeOut     =  %d\n", es_event->TimeOut);
			break;
		case UPNP_EVENT_AUTORENEWAL_FAILED:
		case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
			es_event = (struct Upnp_Event_Subscribe *) event;
			printf("SID         =  %s\n", es_event->Sid);
			printf("ErrCode     =  %d\n", es_event->ErrCode);
			printf("PublisherURL=  %s\n", es_event->PublisherUrl);
			printf("TimeOut     =  %d\n", es_event->TimeOut);
			break;
	}
	return 0;
}

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
	if (rc != UPNP_E_SUCCESS) {
		debugf("UpnpAddToActionResponse() failed");
		request->errcode = UPNP_SOAP_E_ACTION_FAILED;
		return -1;
	}
	return 0;
}

int upnp_notify (device_service_t *service, service_variable_t *variable)
{
	int rc;
	const char *variable_name[1] = { variable->name };
	const char *variable_value[1] = { variable->value };
	debugf("sending notify for '%s'='%s' on '%s'", variable->name, variable->value, service->id);
	rc = UpnpNotify(service->device->handle, service->device->uuid, service->id, variable_name, variable_value, 1);
	if (rc != UPNP_E_SUCCESS) {
		debugf("upnp_notify() failed");
	}
	return rc;
}
