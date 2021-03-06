/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 - 2010 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 - 2010 CoreCodec, Inc., http://www.CoreCodec.com
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

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

typedef enum {
	OPT_IPADDR    = 0,
	OPT_NETMASK   = 1,
} controller_options_t;

static char *controller_options[] = {
	"ipaddr",
	"netmask",
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

static device_description_t controller_devices[] = {
	{ "urn:schemas-upnp-org:device:MediaServer:1", mediaserver_services },
	{ "urn:schemas-upnp-org:device:MediaRenderer:1", mediarenderer_services },
	{ "upnp:rootdevice", mediaserver_services },
	{ "upnp:rootdevice", mediarenderer_services },
	{ NULL },
};

static client_t controller = {
	"controller",
	controller_devices,
};

client_t * upnpd_controller_init (char *options)
{
	int rc;
	int err;
	char *value;
	char *netmask;
	char *ipaddr;
	char *suboptions;

	err = 0;
	netmask = NULL;
	ipaddr = NULL;
	suboptions = options;
	debugf(_DBG, "options: %s\n", options);
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, controller_options, &value)) {
			case OPT_IPADDR:
				if (value == NULL) {
					debugf(_DBG, "value is missing for ipaddr option");
					err = 1;
					continue;
				}
				ipaddr = value;
				break;
			case OPT_NETMASK:
				if (value == NULL) {
					debugf(_DBG, "value is missing for ipaddr option");
					err = 1;
					continue;
				}
				netmask = value;
				break;
		}
	}

	debugf(_DBG, "starting controller;\n"
	       "\tipaddr  : %s\n"
	       "\tnetmask : %s\n",
	       (ipaddr) ? ipaddr : "null",
	       (netmask) ? netmask : "null");

	controller.ipaddr = ipaddr;
	controller.ifmask = netmask;

	debugf(_DBG, "initializing controller client");
	rc = upnpd_client_init(&controller);
	if (rc != 0) {
		debugf(_DBG, "upnpd_client_init(&controller) failed");
		return NULL;
	}
	debugf(_DBG, "initialized controller client");
	return &controller;
}

int upnpd_controller_uninit (client_t *controller)
{
	debugf(_DBG, "uninitializing controller client");
	upnpd_client_uninit(controller);
	debugf(_DBG, "uninitialized controller client");
	return 0;
}

typedef struct controller_parser_data_s {
	uint32_t totalmatches;
	uint32_t numberreturned;
	uint32_t updateid;
	entry_t *entry;
} controller_parser_data_t;

static int controller_parser_callback (void *context, const char *path, const char *name, const char **atrr, const char *value)
{
	entry_t *pentry;
	entry_t *tentry;
	controller_parser_data_t *data;
	data = (controller_parser_data_t *) context;
	if (value == NULL) {
		return 0;
	}
	if (strcmp(path, "/s:Envelope/s:Body/u:BrowseResponse/TotalMatches") == 0 ||
	    strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:BrowseResponse/TotalMatches") == 0) {
		data->totalmatches = upnpd_strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:BrowseResponse/NumberReturned") == 0 ||
	           strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:BrowseResponse/NumberReturned") == 0) {
		data->numberreturned = upnpd_strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:BrowseResponse/UpdateID") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:BrowseResponse/UpdateID") == 0) {
		data->updateid = upnpd_strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:BrowseResponse/Result") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:BrowseResponse/Result") == 0) {
		tentry = upnpd_entry_from_result(value);
		if (tentry == NULL) {
			debugf(_DBG, "upnpd_entry_from_result() failed");
		} else {
			if (data->entry == NULL) {
				data->entry = tentry;
			} else {
				pentry = data->entry;
				while (1) {
					if (pentry->next == NULL) {
						pentry->next = tentry;
						break;
					}
					pentry = pentry->next;
				}
			}
		}
	}
	return 0;
}

entry_t * upnpd_controller_browse_children (client_t *controller, const char *device, const char *object)
{
	char *params[6];
	char *values[6];
	char *action;

	uint32_t count;
	controller_parser_data_t data;

	count = 0;
	action = NULL;
	memset(&data, 0, sizeof(data));

	params[0] = "ObjectID";
	params[1] = "BrowseFlag";
	params[2] = "Filter";
	params[3] = "StartingIndex";
	params[4] = "RequestedCount";
	params[5] = "SortCriteria";

	values[3] = (char *) malloc(sizeof(char) * 40);
	if (values[3] == NULL) {
		debugf(_DBG, "malloc failed");
		goto out;
	}

	do {
		values[0] = (char *) object;
		values[1] = "BrowseDirectChildren";
		values[2] = "*";
		sprintf(values[3], "%u", count);
		values[4] = "50";
		values[5] = "+dc:title";

		debugf(_DBG, "browsing '%s':'%s'", device, object);
		action = upnpd_client_action(controller, device, "urn:schemas-upnp-org:service:ContentDirectory:1", "Browse", params, values, 6);
		if (action == NULL) {
			debugf(_DBG, "upnpd_client_action() failed");
			goto out;
		}
		data.numberreturned = 0;
		data.totalmatches = 0;
		data.updateid = 0;
		if (upnpd_xml_parse_buffer_callback(action, strlen(action), controller_parser_callback, &data) != 0) {
			debugf(_DBG, "upnpd_xml_parse_buffer_callback() failed");
			free(action);
			goto out;
		}
		free(action);
		if (data.totalmatches == 0 || data.numberreturned == 0) {
			debugf(_DBG, "total matches (%d) or number returned (%d) is zero", data.totalmatches, data.numberreturned);
			goto out;
		}
		count += data.numberreturned;
		if (count >= data.totalmatches) {
			goto out;
		}
	} while (1);

out:
	free(values[3]);
	return data.entry;
}

entry_t * upnpd_controller_browse_metadata (client_t *controller, const char *device, const char *object)
{
	char *params[6];
	char *values[6];
	char *action;
	controller_parser_data_t data;

	action = NULL;
	memset(&data, 0, sizeof(data));

	params[0] = "ObjectID";
	params[1] = "BrowseFlag";
	params[2] = "Filter";
	params[3] = "StartingIndex";
	params[4] = "RequestedCount";
	params[5] = "SortCriteria";

	values[0] = (char *) object;
	values[1] = "BrowseMetadata";
	values[2] = "*";
	values[3] = "0";
	values[4] = "0";
	values[5] = "+dc:title";

	debugf(_DBG, "browsing '%s':'%s'", device, object);
	action = upnpd_client_action(controller, device, "urn:schemas-upnp-org:service:ContentDirectory:1", "Browse", params, values, 6);
	if (action == NULL) {
		debugf(_DBG, "upnpd_client_action() failed");
		goto out;
	}
	memset(&data, 0, sizeof(data));
	if (upnpd_xml_parse_buffer_callback(action, strlen(action), controller_parser_callback, &data) != 0) {
		debugf(_DBG, "upnpd_xml_parse_buffer_callback() failed");
	}
	free(action);
out:
	return data.entry;
}
