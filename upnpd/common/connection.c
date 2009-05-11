/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 - 2009 by Alper Akcan
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
#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>

#include "gena.h"
#include "upnp.h"
#include "common.h"

/**
  */
typedef struct connection_s {
	/** */
	device_service_t service;
} connection_t;

/** instance declerations
  */
#define CONNECTION_INSTANCE_MAX 65536

static int instance_ids[CONNECTION_INSTANCE_MAX];
static list_t instance_list;
static list_t mimetype_list;

/**
  */
typedef struct connection_instance_s {
	/** */
	list_t head;
	/** */
	uint32_t connectionid;
	/** */
	uint32_t transportid;
	/** */
	uint32_t rcsid;
	/** */
	char *remoteprotocolinfo;
	/** */
	uint32_t peerconnectionid;
	/** */
	char *peerconnectionmanager;
	/** */
	char *direction;
} connection_instance_t;

/**
  */
typedef struct connection_mimetype_s {
	/** */
	list_t head;
	/** */
	char *mimetype;
} connection_mimetype_t;

static int connection_instance_uninit (connection_instance_t *instance)
{
	list_del(&instance->head);
	instance_ids[instance->connectionid] = 0;
	free(instance->remoteprotocolinfo);
	free(instance->peerconnectionmanager);
	free(instance->direction);
	free(instance);
	return 0;
}

static uint32_t connection_instance_init (uint32_t instanceid)
{
	connection_instance_t *inst;
	if (instance_ids[instanceid] == 1) {
		debugf("instanceid '%u' is in use", instanceid);
		return (uint32_t) -1;
	}
	inst = (connection_instance_t *) malloc(sizeof(connection_instance_t));
	if (inst == NULL) {
		debugf("malloc() failed");
		return (uint32_t) -1;
	}
	memset(inst, 0, sizeof(connection_instance_t));
	list_add(&inst->head, &instance_list);
	inst->connectionid = instanceid;
	instance_ids[instanceid] = 1;
	debugf("created new connection instance '%u'", inst->connectionid);
	return inst->connectionid;
}

uint32_t connection_instance_new (void)
{
	uint32_t i;
	/* start from 1, 0 is reserved */
	for (i = 1; i < CONNECTION_INSTANCE_MAX; i++) {
		if (instance_ids[i] == 0) {
			return connection_instance_init(i);
		}
	}
	return (uint32_t) -1;
}

connection_instance_t * connection_instance_get (uint32_t id)
{
	connection_instance_t *inst;
	list_for_each_entry(inst, &instance_list, head) {
		if (instance_ids[id] == 1 && inst->connectionid == id) {
			return inst;
		}
	}
	return NULL;
}

static int connectionmanager_get_protocol_info (device_service_t *service, upnp_event_action_t *request)
{
	service_variable_t *variable;
	debugf("connectionmanager get protocol info");
	variable = service_variable_find(service, "SourceProtocolInfo");
	if (variable != NULL) {
		upnp_add_response(request, service->type, "Source", variable->value);
	}
	variable = service_variable_find(service, "SinkProtocolInfo");
	if (variable != NULL) {
		upnp_add_response(request, service->type, "Sink", variable->value);
	}
	return 0;
}

static int connectionmanager_get_current_connection_ids (device_service_t *service, upnp_event_action_t *request)
{
	service_variable_t *variable;
	debugf("connectionmanager get current connection ids");
	variable = service_variable_find(service, "CurrentConnectionIDs");
	if (variable != NULL) {
		upnp_add_response(request, service->type, "ConnectionIDs", variable->value);
	}
	return 0;
}

static int connectionmanager_get_current_connection_info (device_service_t *service, upnp_event_action_t *request)
{
	uint32_t connectionid;
	connection_instance_t *cinstance;

	char str[23];
	char *protocolinfo;
	char *peerconnectionmanager;
	char *direction;
	char *status;

	debugf("connectionmanager get current connection info");
	connectionid = xml_get_ui4(request->request, "ConnectionID");
	cinstance = connection_instance_get(connectionid);
	if (cinstance == NULL) {
		request->errcode = UPNP_ERROR_PARAMETER_MISMATCH;
		return 0;
	}

	protocolinfo = xml_escape(cinstance->remoteprotocolinfo, 0);
	peerconnectionmanager = xml_escape(cinstance->peerconnectionmanager, 0);
	direction = xml_escape(cinstance->direction, 0);
	status = "Unknown";

	upnp_add_response(request, service->type, "RcsID", uint32tostr(str, cinstance->rcsid));
	upnp_add_response(request, service->type, "AVTransportID", uint32tostr(str, cinstance->transportid));
	upnp_add_response(request, service->type, "ProtocolInfo", protocolinfo);
	upnp_add_response(request, service->type, "PeerConnectionManager", peerconnectionmanager);
	upnp_add_response(request, service->type, "PeerConnectionID", uint32tostr(str, cinstance->peerconnectionid));
	upnp_add_response(request, service->type, "Direction", direction);
	upnp_add_response(request, service->type, "Status", status);

	free(protocolinfo);
	free(peerconnectionmanager);
	free(direction);

	return 0;
}

#if defined(ENABLE_OPTIONAL)
static int strprintf (char **str, char *fmt, ...)
{
	char *atmp;
	char *vtmp;
	va_list va;
	va_start(va, fmt);
	if (vasprintf(&vtmp, fmt, va) < 0) {
		va_end(va);
		return -1;
	}
	va_end(va);
	if (*str == NULL) {
		*str = vtmp;
		return 0;
	} else {
		if (asprintf(&atmp, "%s%s", *str, vtmp) < 0) {
			free(vtmp);
			return -1;
		} else {
			free(vtmp);
			free(*str);
			*str = atmp;
			return 0;
		}
	}
	return -1;
}

static int connectionmanager_notify_currentids (device_service_t *service)
{
	char *str;
	service_variable_t *currentids;
	connection_instance_t *cinstance;
	currentids = service_variable_find(service, "CurrentConnectionIDs");
	if (currentids != NULL) {
		str = NULL;
		list_for_each_entry(cinstance, &instance_list, head) {
			if (list_is_last(&cinstance->head, &instance_list) == 1) {
				strprintf(&str, "%u", cinstance->connectionid);
			} else {
				strprintf(&str, "%u,", cinstance->connectionid);
			}
		}
		free(currentids->value);
		if (str == NULL) {
			str = strdup("");
		}
		currentids->value = str;
	}
	upnp_notify(service, currentids);
	return 0;
}

static int connectionmanager_prepare_for_connection (device_service_t *service, struct Upnp_Action_Request *request)
{
	int ret;
	char str[23];

	connection_t *connection;
	connection_instance_t *cinstance;

	char *remoteprotocolinfo;
	char *peerconnectionmanager;
	uint32_t peerconnectionid;
	char *direction;


	connection = (connection_t *) service;

	ret = 0;

	remoteprotocolinfo = xml_get_string(request->ActionRequest, "RemoteProtocolInfo");
	peerconnectionmanager = xml_get_string(request->ActionRequest, "PeerConnectionManager");
	peerconnectionid = xml_get_ui4(request->ActionRequest, "PeerConnectionID");
	direction = xml_get_string(request->ActionRequest, "Direction");

	debugf("connectionmanager prepare for connection;"
	       "  remoteprotocolinfo   : %s\n"
	       "  peerconnectionmanager: %s\n"
	       "  peerconnectionid     : %u\n"
	       "  direction            : %s\n",
	       remoteprotocolinfo,
	       peerconnectionmanager,
	       peerconnectionid,
	       direction);

	cinstance = connection_instance_get(connection_instance_new());
	if (cinstance == NULL) {
		debugf("connection_instance_get() failed");
		ret = -1;
		goto out;
	}
	cinstance->remoteprotocolinfo = (remoteprotocolinfo) ? strdup(remoteprotocolinfo) : NULL;
	cinstance->peerconnectionid = peerconnectionid;
	cinstance->peerconnectionmanager = (peerconnectionmanager) ? strdup(peerconnectionmanager) : NULL;
	cinstance->direction = (direction) ? strdup(direction) : NULL;
	cinstance->transportid = transport_instance_new(service->device);
	cinstance->rcsid = 0;

	debugf("cinstance\n"
		"  remoteprotocolinfo   : %s\n"
		"  peerconnectionid     : %u\n"
		"  peerconnectionmanager: %s\n"
		"  direction            : %s\n"
		"  connectionid         : %u\n"
		"  transportid          : %u\n"
		"  rcsid                : %u\n",
		cinstance->remoteprotocolinfo,
		cinstance->peerconnectionid,
		cinstance->peerconnectionmanager,
		cinstance->direction,
		cinstance->connectionid,
		cinstance->transportid,
		cinstance->rcsid);

	upnp_add_response(request, service->type, "ConnectionID", uint32tostr(str, cinstance->connectionid));
	upnp_add_response(request, service->type, "AVTransportID", uint32tostr(str, cinstance->transportid));
	upnp_add_response(request, service->type, "RcsID", uint32tostr(str, cinstance->rcsid));

	connectionmanager_notify_currentids(service);

out:	free(remoteprotocolinfo);
	free(peerconnectionmanager);
	free(direction);
	return ret;
}

static int connectionmanager_connection_complete (device_service_t *service, struct Upnp_Action_Request *request)
{
	uint32_t connectionid;
	connection_instance_t *cinstance;

	debugf("connectionmanager connection complete");
	connectionid = xml_get_ui4(request->ActionRequest, "ConnectionID");
	cinstance = connection_instance_get(connectionid);
	if (cinstance == NULL) {
		request->ErrCode = 718;
		sprintf(request->ErrStr, "Invalid InstanceID");
		return 0;
	}

	transport_instance_delete(service->device, cinstance->transportid);
	connection_instance_uninit(cinstance);
	connectionmanager_notify_currentids(service);

	return 0;
}
#endif

static char *allowed_values_connectionstatus[] = {
	"OK",
	"ContentFormatMismatch",
	"InsufficientBandwitdh",
	"UnreliableChannel",
	"Unknown",
	NULL,
};

static char *allowed_values_direction[] = {
	"Output",
	"Input",
	NULL,
};

static service_variable_t *connectionmanager_variables[] = {
	/* required */
	& (service_variable_t) {"SourceProtocolInfo", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_YES, NULL, NULL},
	& (service_variable_t) {"SinkProtocolInfo", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_YES, NULL, NULL},
	& (service_variable_t) {"CurrentConnectionIDs", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_YES, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_ConnectionStatus", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_connectionstatus, NULL},
	& (service_variable_t) {"A_ARG_TYPE_ConnectionManager", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Direction", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_direction, NULL},
	& (service_variable_t) {"A_ARG_TYPE_ProtocolInfo", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_ConnectionID", VARIABLE_DATATYPE_I4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_AVTransportID", VARIABLE_DATATYPE_I4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_RcsID", VARIABLE_DATATYPE_I4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	NULL,
};

static action_argument_t *arguments_get_protocol_info[] = {
	& (action_argument_t) {"Source", ARGUMENT_DIRECTION_OUT, "SourceProtocolInfo"},
	& (action_argument_t) {"Sink", ARGUMENT_DIRECTION_OUT, "SinkProtocolInfo"},
	NULL,
};

#if defined(ENABLE_OPTIONAL)
static action_argument_t *arguments_prepare_for_connection[] = {
	& (action_argument_t) {"RemoteProtocolInfo", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ProtocolInfo"},
	& (action_argument_t) {"PeerConnectionManager", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ConnectionManager"},
	& (action_argument_t) {"PeerConnectionID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ConnectionID"},
	& (action_argument_t) {"Direction", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_Direction"},
	& (action_argument_t) {"ConnectionID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_ConnectionID"},
	& (action_argument_t) {"AVTransportID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_AVTransportID"},
	& (action_argument_t) {"RcsID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_RcsID"},
	NULL,
};

static action_argument_t *arguments_connection_complete[] = {
	& (action_argument_t) {"ConnectionID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ConnectionID"},
	NULL,
};
#endif

static action_argument_t *arguments_get_current_connection_ids[] = {
	& (action_argument_t) {"ConnectionIDs", ARGUMENT_DIRECTION_OUT, "CurrentConnectionIDs"},
	NULL,
};

static action_argument_t *arguments_get_current_connection_info[] = {
	& (action_argument_t) {"ConnectionID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ConnectionID"},
	& (action_argument_t) {"RcsID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_RcsID"},
	& (action_argument_t) {"AVTransportID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_AVTransportID"},
	& (action_argument_t) {"ProtocolInfo", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_ProtocolInfo"},
	& (action_argument_t) {"PeerConnectionManager", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_ConnectionManager"},
	& (action_argument_t) {"PeerConnectionID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_ConnectionID"},
	& (action_argument_t) {"Direction", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_Direction"},
	& (action_argument_t) {"Status", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_ConnectionStatus"},
	NULL,
};

static service_action_t *connectionmanager_actions[] = {
	/* required */
	& (service_action_t) {"GetProtocolInfo", arguments_get_protocol_info, connectionmanager_get_protocol_info},
	& (service_action_t) {"GetCurrentConnectionIDs", arguments_get_current_connection_ids, connectionmanager_get_current_connection_ids},
	& (service_action_t) {"GetCurrentConnectionInfo", arguments_get_current_connection_info, connectionmanager_get_current_connection_info},
	/* optional */
#if defined(ENABLE_OPTIONAL)
	& (service_action_t) {"PrepareForConnection", arguments_prepare_for_connection, connectionmanager_prepare_for_connection},
	& (service_action_t) {"ConnectionComplete", arguments_connection_complete, connectionmanager_connection_complete},
#endif
	NULL,
};

static int connectionmanager_register_mimetype_actual (device_service_t *service, const char *mime)
{
	char *tmp;
	connection_mimetype_t *mt;
	service_variable_t *variable;

	list_for_each_entry(mt, &mimetype_list, head) {
		if (strcmp(mt->mimetype, mime) == 0) {
			return 0;
		}
	}

	mt = (connection_mimetype_t *) malloc(sizeof(connection_mimetype_t));
	if (mt == NULL) {
		debugf("malloc() failed");
		return -1;
	}

	debugf("registering mime type '%s'", mime);
	memset(mt, 0, sizeof(connection_mimetype_t));
	list_add(&mt->head, &mimetype_list);
	mt->mimetype = strdup(mime);

	debugf("changing source protocol info variable");
	variable = service_variable_find(service, "SourceProtocolInfo");
	if (variable != NULL) {
		if (variable->value == NULL) {
			/* sony shit */
			variable->value = strdup(
					"http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN,"
					"http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_SM;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_PS_NTSC;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_PS_PAL;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_HD_NA_ISO;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_T;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:video/x-ms-wmv:DLNA.ORG_PN=WMVHIGH_PRO;DLNA.ORG_OP=01;DLNA.ORG_CI=0,"
					"http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_OP=01,"
					"http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMABASE;DLNA.ORG_OP=01,"
					"http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMAFULL;DLNA.ORG_OP=01,"
					"http-get:*:audio/x-ms-wma:DLNA.ORG_PN=WMAPRO;DLNA.ORG_OP=01,"
					"http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO_320;DLNA.ORG_OP=01,"
					"http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO;DLNA.ORG_OP=01,"
					"http-get:*:audio/mp4:DLNA.ORG_PN=AAC_MULT5_ISO;DLNA.ORG_OP=01,"
					"http-get:*:audio/mp4:*,"
					"http-get:*:audio/wav:*,"
					"http-get:*:audio/x-aiff:*,"
					"http-get:*:audio/x-flac:*,"
					"http-get:*:application/ogg:*,"
					"http-get:*:image/jpeg:*,"
					"http-get:*:image/gif:*,"
					"http-get:*:audio/x-mpegurl:*,"
					"http-get:*:video/mpeg:*,"
					"http-get:*:video/x-msvideo:*,"
					"http-get:*:video/avi:*,"
					"http-get:*:video/mpeg2:*,"
					"http-get:*:video/dvd:*,"
					"http-get:*:video/x-ms-wmv:*"
					);
		}
		if (variable->value != NULL) {
			tmp = variable->value;
			if (asprintf(&variable->value, "%s,http-get:*:%s:*", tmp, mime) < 0) {
				debugf("asprintf() failed");
				variable->value = tmp;
				return -1;
			}
			free(tmp);
		} else {
			if (asprintf(&variable->value, "http-get:*:%s:*", mime) < 0) {
				debugf("asprintf() failed");
				variable->value = strdup("");
				return -1;
			}
		}
	}
	variable = service_variable_find(service, "SinkProtocolInfo");
	if (variable != NULL) {
		if (variable->value != NULL) {
			tmp = variable->value;
			if (asprintf(&variable->value, "%s,http-get:*:%s:*", tmp, mime) < 0) {
				debugf("asprintf() failed");
				variable->value = tmp;
				return -1;
			}
			free(tmp);
		} else {
			if (asprintf(&variable->value, "http-get:*:%s:*", mime) < 0) {
				debugf("asprintf() failed");
				variable->value = strdup("");
				return -1;
			}
		}
	}
	return 0;
}

int connectionmanager_register_mimetype (device_service_t *service, const char *mime)
{
	connectionmanager_register_mimetype_actual(service, mime);
	if (strcmp("audio/mpeg", mime) == 0) {
		connectionmanager_register_mimetype_actual(service, "audio/x-mpeg");
	}
	return 0;
}

int connectionmanager_uninit (device_service_t *service)
{
	int i;
	connection_mimetype_t *mime;
	connection_instance_t *inst;
	connection_mimetype_t *mnext;
	connection_instance_t *inext;
	service_variable_t *variable;
	debugf("uninitializing connection manager service");
	list_for_each_entry_safe(inst, inext, &instance_list, head) {
		debugf("uninitializing connection instance '%u'", inst->connectionid);
		connection_instance_uninit(inst);
	}
	debugf("uninitializing mimetype list");
	list_for_each_entry_safe(mime, mnext, &mimetype_list, head) {
		free(mime->mimetype);
		free(mime);
	}
	debugf("uninitializing service variables");
	for (i = 0; (variable = service->variables[i]) != NULL; i++) {
		free(variable->value);
	}
	free(service);
	return 0;
}

device_service_t * connectionmanager_init (void)
{
	int i;
	connection_t *connection;
	service_variable_t *variable;
	debugf("initializing connection manager service struct");
	connection = (connection_t *) malloc(sizeof(connection_t));
	if (connection == NULL) {
		debugf("malloc(sizeof(connection_t)) failed");
		goto out;
	}
	memset(connection, 0, sizeof(connection_t));
	connection->service.name = "connectionmanager";
	connection->service.id = "urn:upnp-org:serviceId:ConnectionManager";
	connection->service.type = "urn:schemas-upnp-org:service:ConnectionManager:1";
	connection->service.scpdurl = "/upnp/connectionmanager.xml";
	connection->service.eventurl = "/upnp/event/connectionmanager1";
	connection->service.controlurl = "/upnp/control/connectionmanager1";
	connection->service.actions = connectionmanager_actions;
	connection->service.variables = connectionmanager_variables;
	connection->service.uninit = connectionmanager_uninit;

	debugf("initializing connection manager service");
	if (service_init(&connection->service) != 0) {
		debugf("service_init(&connection->service) failed");
		free(connection);
		connection = NULL;
		goto out;
	}

	list_init(&mimetype_list);

	debugf("initializing ac transport instances");
	for (i = 0; i < CONNECTION_INSTANCE_MAX; i++) {
		instance_ids[i] = 0;
	}
	list_init(&instance_list);

	variable = service_variable_find(&connection->service, "CurrentConnectionIDs");
	if (variable != NULL) {
#if defined(ENABLE_OPTIONAL)
		variable->value = strdup("");
#else
		connection_instance_t *cinstance;
		cinstance = connection_instance_get(connection_instance_init(0));
		if (cinstance == NULL) {
			debugf("creating special instance '0' failed");
			free(connection);
			connection = NULL;
			goto out;
		}
		cinstance->connectionid = 0;
		cinstance->direction = strdup("Input");
		cinstance->peerconnectionid = -1;
		cinstance->peerconnectionmanager = NULL;
		cinstance->rcsid = 0;
		cinstance->remoteprotocolinfo = NULL;
		cinstance->transportid = 0;
		variable->value = strdup("0");
#endif
	}

	debugf("initialized connection manager service");
out:	return (device_service_t *) connection;
}
