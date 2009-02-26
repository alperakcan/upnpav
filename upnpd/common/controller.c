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

typedef enum {
	OPT_INTERFACE    = 0,
} mediaserver_options_t;

static char *mediaserver_options[] = {
	[OPT_INTERFACE]    = "interface",
	NULL,
};

static char *mediaserver_services[] = {
	"urn:schemas-upnp-org:service:ContentDirectory:1",
	"urn:schemas-upnp-org:service:ConnectionManager:1",
	"urn:schemas-upnp-org:service:AVTransport:1",
	NULL,
};

static char *mediarenderer_services[] = {
	"urn:schemas-upnp-org:service:ConnectionManager:1",
	"urn:schemas-upnp-org:service:AVTransport:1",
	"urn:schemas-upnp-org:service:RenderingControl:1",
	NULL,
};

static device_description_t *controller_devices[] = {
	& (device_description_t) { "urn:schemas-upnp-org:device:MediaServer:1", mediaserver_services },
	& (device_description_t) { "urn:schemas-upnp-org:device:MediaRenderer:1", mediarenderer_services },
	NULL,
};

static client_t controller = {
	.name = "controller",
	.descriptions = controller_devices,
};

client_t * controller_init (char *options)
{
	int rc;
	int err;
	char *value;
	char *interface;
	char *suboptions;

	err = 0;
	interface = NULL;
	suboptions = options;
	debugf("%s\n", options);
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, mediaserver_options, &value)) {
			case OPT_INTERFACE:
				if (value == NULL) {
					debugf("value is missing for interface option");
					err = 1;
					continue;
				}
				interface = value;
				break;
		}
	}

	debugf("starting controller;\n"
	       "\tinterface   : %s\n",
	       (interface) ? interface : "null");

	controller.interface = interface;

	debugf("initializing controller client");
	rc = client_init(&controller);
	if (rc != 0) {
		debugf("client_init(&controller) failed");
		return NULL;
	}
	debugf("initialized controller client");
	return &controller;
}

int controller_uninit (client_t *controller)
{
	debugf("uninitializing controller client");
	client_uninit(controller);
	debugf("uninitialized controller client");
	return 0;
}

static int controller_play_match (char *source, char *sink)
{
	debugf("checking if '%s' matches '%s'", source, sink);
	return 0;
}

int controller_play_object (client_t *controller, char *object, char *server, char *renderer)
{
	int ret;
	char *sink;
	char *source;
	entry_t *entry;
	char *params[10];
	char *values[10];
	IXML_Document *response;

	ret = 0;
	entry = NULL;
	sink = NULL;
	source = NULL;
	response = NULL;

	debugf("playing %s on %s, from %s", object, renderer, server);
	entry = controller_browse_metadata(controller, server, object);
	if (entry == NULL) {
		debugf("controller_browse_metadata(%s, %s) failed", server, object);
		ret = -1;
		goto out;
	}
	debugf("object information:");
	entry_dump(entry);

	debugf("calling getprotocolinfo on %s", renderer);
	response = client_action(controller, renderer, "urn:schemas-upnp-org:service:ConnectionManager:1", "GetProtocolInfo", NULL, NULL, 0);
	if (response == NULL) {
		debugf("client_action() failed");
		ret = -1;
		goto out;
	}
	source = xml_get_first_document_item(response, "Source");
	sink = xml_get_first_document_item(response, "Sink");
	debugf("choosing matching protocol/format");
	debugf("  source: %s\n", source);
	debugf("  sink  : %s\n", sink);
	if (controller_play_match(entry->didl.res.protocolinfo, sink) != 0) {
		debugf("could not find matching protocol/info");
		ret = -1;
		goto out;
	}
	ixmlDocument_free(response);
	response = NULL;

	debugf("calling prepareforconnection on %s", server);
	response = client_action(controller, server, "urn:schemas-upnp-org:service:ConnectionManager:1", "PrepareForConnection", NULL, NULL, 0);
	if (response == NULL) {
		debugf("server did not implemented prepare for connection");
	}
	ixmlDocument_free(response);
	response = NULL;

	debugf("calling prepareforconnection on %s", renderer);
	response = client_action(controller, renderer, "urn:schemas-upnp-org:service:ConnectionManager:1", "PrepareForConnection", NULL, NULL, 0);
	if (response == NULL) {
		debugf("render did not implemented prepare for connection");
	}
	ixmlDocument_free(response);
	response = NULL;

	debugf("calling setavtransporturi on %s", renderer);
	params[0] = "InstanceID";
	params[1] = "CurrentURI";
	params[2] = "CurrentURIMetaData";
	values[0] = "0";
	values[1] = entry->didl.res.path;
	values[2] = NULL;
	response = client_action(controller, renderer, "urn:schemas-upnp-org:service:AVTransport:1", "SetAVTransportURI", params, values, 3);
	if (response == NULL) {
		debugf("setavtransporturi failed on %s", renderer);
		ret = -1;
		goto out;
	}
	ixmlDocument_free(response);
	response = NULL;

	debugf("calling play on %s", renderer);
	params[0] = "InstanceID";
	params[1] = "Speed";
	values[0] = "0";
	values[1] = "1";
	response = client_action(controller, renderer, "urn:schemas-upnp-org:service:AVTransport:1", "Play", params, values, 2);
	if (response == NULL) {
		debugf("play failed on %s", renderer);
		ret = -1;
		goto out;
	}
	ixmlDocument_free(response);
	response = NULL;

out:	free(source);
	free(sink);
	entry_uninit(entry);
	ixmlDocument_free(response);
	return 0;
}

entry_t * controller_browse_children (client_t *controller, char *device, char *object)
{
	char *tmp;
	char *result;
	entry_t *entry;
	entry_t *tentry;
	entry_t *pentry;
	char *params[6];
	char *values[6];
	IXML_Document *response;

	uint32_t count;
	uint32_t totalmatches;
	uint32_t numberreturned;
	uint32_t updateid;

	count = 0;
	entry = NULL;
	result = NULL;
	response = NULL;

	params[0] = "ObjectID";
	params[1] = "BrowseFlag";
	params[2] = "Filter";
	params[3] = "StartingIndex";
	params[4] = "RequestedCount";
	params[5] = "SortCriteria";

	values[3] = (char *) malloc(sizeof(char) * 40);
	if (values[3] == NULL) {
		debugf("malloc failed");
		goto out;
	}

	do {
		values[0] = object;
		values[1] = "BrowseDirectChildren";
		values[2] = "*";
		sprintf(values[3], "%u", count);
		values[4] = "5";
		values[5] = "+dc:title";

		debugf("browsing '%s':'%s'", device, object);
		response = client_action(controller, device, "urn:schemas-upnp-org:service:ContentDirectory:1", "Browse", params, values, 6);
		if (response == NULL) {
			debugf("client_action() failed");
			goto out;
		}

#if 0
		{
			char *tmp;
			tmp = ixmlDocumenttoString(response);
			printf("%s\n", tmp);
			free(tmp);
		}
#endif

		tmp = xml_get_first_document_item(response, "TotalMatches");
		totalmatches = strtouint32(tmp);
		free(tmp);
		tmp = xml_get_first_document_item(response, "NumberReturned");
		numberreturned = strtouint32(tmp);
		free(tmp);
		tmp = xml_get_first_document_item(response, "UpdateID");
		updateid = strtouint32(tmp);
		free(tmp);

		result = xml_get_first_document_item(response, "Result");
		if (result == NULL) {
			debugf("xml_get_first_document_item(response, Result) failed");
			goto out;
		}

		tentry = entry_from_result(result);
		if (tentry == NULL) {
			debugf("entry_from_result() failed");
			goto out;
		}
		if (entry == NULL) {
			entry = tentry;
		} else {
			pentry = entry;
			while (1) {
				if (pentry->next == NULL) {
					pentry->next = tentry;
					pentry->next->prev = pentry;
					break;
				}
				pentry = pentry->next;
			}
		}

		if (totalmatches == 0 || numberreturned == 0) {
			debugf("total matches (%d) or number returned (%d) is zero", totalmatches, numberreturned);
			goto out;
		}
		count += numberreturned;
		if (count >= totalmatches) {
			goto out;
		}

		free(result);
		ixmlDocument_free(response);
		result = NULL;
		response = NULL;
	} while (1);

out:	if (result) {
		free(result);
	}
	if (values[3]) {
		free(values[3]);
	}
	ixmlDocument_free(response);
	return entry;
}

entry_t * controller_browse_metadata (client_t *controller, char *device, char *object)
{
	entry_t *entry;
	char *tmp;
	char *result;
	char *params[6];
	char *values[6];
	IXML_Document *response;

	uint32_t totalmatches;
	uint32_t numberreturned;
	uint32_t updateid;

	entry = NULL;
	result = NULL;

	params[0] = "ObjectID";
	params[1] = "BrowseFlag";
	params[2] = "Filter";
	params[3] = "StartingIndex";
	params[4] = "RequestedCount";
	params[5] = "SortCriteria";

	values[0] = object;
	values[1] = "BrowseMetadata";
	values[2] = "*";
	values[3] = "0";
	values[4] = "0";
	values[5] = "+dc:title";

	debugf("browsing '%s':'%s'", device, object);
	response = client_action(controller, device, "urn:schemas-upnp-org:service:ContentDirectory:1", "Browse", params, values, 6);
	if (response == NULL) {
		debugf("client_action() failed");
		goto out;
	}

#if 0
	{
		char *tmp;
		tmp = ixmlDocumenttoString(response);
		printf("%s\n", tmp);
		free(tmp);
	}
#endif

	tmp = xml_get_first_document_item(response, "TotalMatches");
	totalmatches = strtouint32(tmp);
	free(tmp);
	tmp = xml_get_first_document_item(response, "NumberReturned");
	numberreturned = strtouint32(tmp);
	free(tmp);
	tmp = xml_get_first_document_item(response, "UpdateID");
	updateid = strtouint32(tmp);
	free(tmp);

	result = xml_get_first_document_item(response, "Result");
	if (result == NULL) {
		debugf("xml_get_first_document_item(response, Result) failed");
		goto out;
	}

	entry = entry_from_result(result);
	if (entry == NULL) {
		debugf("entry_from_result() failed");
		goto out;
	}

out:	free(result);
	ixmlDocument_free(response);
	return entry;
}