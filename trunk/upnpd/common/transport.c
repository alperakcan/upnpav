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

#define TRANSPORT_SERVICE_NAME	"avtransport"
#define TRANSPORT_SERVICE_ID	"urn:upnp-org:serviceId:AVTransport"
#define TRANSPORT_SERVICE_TYPE	"urn:schemas-upnp-org:service:AVTransport:1"

/**
  */
typedef struct transport_s {
	/** */
	device_service_t service;
	/** */
	render_t *render;
} transport_t;

/** instance declerations
  */
#define TRANSPORT_INSTANCE_MAX 65536

static int instance_ids[TRANSPORT_INSTANCE_MAX];
static list_t instance_list;

static int transport_notify_lastchange (device_t *device, uint32_t instanceid);
static int avtransport_set_avtransport_uri (device_service_t *service, upnp_event_action_t *request);
static int avtransport_get_media_info (device_service_t *service, upnp_event_action_t *request);
static int avtransport_get_transport_info (device_service_t *service, upnp_event_action_t *request);
static int avtransport_get_position_info (device_service_t *service, upnp_event_action_t *request);
static int avtransport_get_device_capabilities (device_service_t *service, upnp_event_action_t *request);
static int avtransport_get_transport_settings (device_service_t *service, upnp_event_action_t *request);
static int avtransport_stop (device_service_t *service, upnp_event_action_t *request);
static int avtransport_play (device_service_t *service, upnp_event_action_t *request);
static int avtransport_seek (device_service_t *service, upnp_event_action_t *request);
static int avtransport_next (device_service_t *service, upnp_event_action_t *request);
static int avtransport_previous (device_service_t *service, upnp_event_action_t *request);

static char *allowed_values_storagemedium[] = {
	/* optional */
#if defined(ENABLE_OPTIONAL)
	"UNKNOWN",
	"DV",
	"MINI-DV",
	"VHS",
	"W-VHS",
	"S-VHS",
	"D-VHS",
	"VHSC",
	"VIDEO8",
	"HI8",
	"CD-ROM",
	"CD-DA",
	"CD-R",
	"CD-RW",
	"VIDEO-CD",
	"SACD",
	"MD-AUDIO",
	"MD-PICTURE",
	"DVD-ROM",
	"DVD-VIDEO",
	"DVD-R",
	"DVD+RW",
	"DVD-RW",
	"DVD-RAM",
	"DVD-AUDIO",
	"DAT",
	"LD",
	"HDD",
	"MICRO-MV",
	"NETWORK",
	"NONE",
	"NOT_IMPLEMENTED",
#endif
	NULL,
};

static char *allowed_values_playmode[] = {
	/* required */
	"NORMAL",
	/* optional */
#if defined(ENABLE_OPTIONAL)
	"SHUFFLE",
	"REPEAT_ONE",
	"REPEAT_ALL",
	"RANDOM",
	"DIRECT_1",
	"INTRO",
#endif
	NULL,
};

static char *allowed_values_playspeed[] = {
	/* required */
	"1",
	NULL,
};

static char *allowed_values_writestatus[] = {
	/*optional */
#if defined(ENABLE_OPTIONAL)
	"WRITABLE",
	"PROTECTED",
	"NOT_WRITABLE",
	"UNKNOWN",
	"NOT_IMPLEMENTED",
#endif
	NULL,
};

static char *allowed_values_recordquality[] = {
	/* optional */
#if defined(ENABLE_OPTIONAL)
	"0:EP",
	"1:LP",
	"2:SP",
	"0:BASIC",
	"1:MEDIUM",
	"2:HIGH",
#endif
	NULL,
};

static char *allowed_values_seekmode[] = {
	/* required */
	"TRACK_NR",
	/* optional */
#if defined(ENABLE_OPTIONAL)
	"ABS_TIME",
	"REL_TIME",
	"ABS_COUNT",
	"REL_COUNT",
	"CHANNEL_FREQ",
	"TAPE-INDEX",
	"FRAME",
#endif
	NULL,
};

static char *allowed_values_transportstate[] = {
	/* required */
	"STOPPED",
	"PLAYING",
	"TRANSITIONING",
	"PAUSED_PLAYBACK",
	"PAUSED_RECORDING",
	"RECORDING",
	"NO_MEDIA_PRESENT",
	NULL,
};

typedef enum {
	TRANSPORT_STATE_STOPPED,
	TRANSPORT_STATE_PLAYING,
	TRANSPORT_STATE_TRANSITIONING,
	TRANSPORT_STATE_PAUSED_PLAYBACK,
	TRANSPORT_STATE_PAUSED_RECORDING,
	TRANSPORT_STATE_RECORDING,
	TRANSPORT_STATE_NO_MEDIA_PRESENT,
	TRANSPORT_STATE_UNKNOWN,
} transport_state_t;

static char *allowed_values_transportstatus[] = {
	/* required */
	"OK",
	"ERROR_OCCURRED",
	NULL,
};

typedef enum {
	TRANSPORT_STATUS_OK,
	TRANSPORT_STATUS_ERROR_OCCURED,
	TRANSPORT_STATUS_UNKNOWN,
} transport_status_t;

static service_variable_t *avtransport_variables[] = {
	/* required */
	& (service_variable_t) {"TransportState", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_transportstate, NULL},
	& (service_variable_t) {"TransportStatus", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_transportstatus, NULL},
	& (service_variable_t) {"PlaybackStorageMedium", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_storagemedium, NULL},
	& (service_variable_t) {"RecordStorageMedium", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_storagemedium, NULL},
	& (service_variable_t) {"PossiblePlaybackStorageMedia", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"PossibleRecordStorageMedia", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"CurrentPlayMode", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_playmode, "NORMAL"},
	& (service_variable_t) {"TransportPlaySpeed", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_playspeed, "1"},
	& (service_variable_t) {"RecordMediumWriteStatus", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_writestatus, NULL},
	& (service_variable_t) {"CurrentRecordQualityMode", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_recordquality, NULL},
	& (service_variable_t) {"PossibleRecordQualityModes", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"NumberOfTracks", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"CurrentTrack", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"CurrentTrackDuration", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"CurrentMediaDuration", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"CurrentTrackMetaData", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"CurrentTrackURI", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"AVTransportURI", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"AVTransportURIMetaData", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"NextAVTransportURI", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"NextAVTransportURIMetaData", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"RelativeTimePosition", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"AbsoluteTimePosition", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"RelativeCounterPosition", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"AbsoluteCounterPosition", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"LastChange", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_YES, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_SeekMode", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_seekmode, NULL},
	& (service_variable_t) {"A_ARG_TYPE_SeekTarget", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_InstanceID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	/* optional */
#if defined(ENABLE_OPTIONAL)
	& (service_variable_t) {"CurrentTransportActions", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
#endif
	NULL,
};

static action_argument_t *arguments_set_avtransport_uri[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"CurrentURI", ARGUMENT_DIRECTION_IN, "AVTransportURI"},
	& (action_argument_t) {"CurrentURIMetaData", ARGUMENT_DIRECTION_IN, "AVTransportURIMetaData"},
	NULL,
};

static action_argument_t *arguments_get_media_info[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"NrTracks", ARGUMENT_DIRECTION_OUT, "NumberOfTracks"},
	& (action_argument_t) {"MediaDuration", ARGUMENT_DIRECTION_OUT, "CurrentMediaDuration"},
	& (action_argument_t) {"CurrentURI", ARGUMENT_DIRECTION_OUT, "AVTransportURI"},
	& (action_argument_t) {"CurrentURIMetaData", ARGUMENT_DIRECTION_OUT, "AVTransportURIMetaData"},
	& (action_argument_t) {"NextURI", ARGUMENT_DIRECTION_OUT, "NextAVTransportURI"},
	& (action_argument_t) {"NextURIMetaData", ARGUMENT_DIRECTION_OUT, "NextAVTransportURIMetaData"},
	& (action_argument_t) {"PlayMedium", ARGUMENT_DIRECTION_OUT, "PlaybackStorageMedium"},
	& (action_argument_t) {"RecordMedium", ARGUMENT_DIRECTION_OUT, "RecordStorageMedium"},
	& (action_argument_t) {"WriteStatus", ARGUMENT_DIRECTION_OUT, "RecordMediumWriteStatus"},
	NULL,
};

static action_argument_t *arguments_get_transport_info[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"CurrentTransportState", ARGUMENT_DIRECTION_OUT, "TransportState"},
	& (action_argument_t) {"CurrentTransportStatus", ARGUMENT_DIRECTION_OUT, "TransportStatus"},
	& (action_argument_t) {"CurrentSpeed", ARGUMENT_DIRECTION_OUT, "TransportPlaySpeed"},
	NULL,
};

static action_argument_t *arguments_get_position_info[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"Track", ARGUMENT_DIRECTION_OUT, "CurrentTrack"},
	& (action_argument_t) {"TrackDuration", ARGUMENT_DIRECTION_OUT, "CurrentTrackDuration"},
	& (action_argument_t) {"TrackMetaData", ARGUMENT_DIRECTION_OUT, "CurrentTrackMetaData"},
	& (action_argument_t) {"TrackURI", ARGUMENT_DIRECTION_OUT, "CurrentTrackURI"},
	& (action_argument_t) {"RelTime", ARGUMENT_DIRECTION_OUT, "RelativeTimePosition"},
	& (action_argument_t) {"AbsTime", ARGUMENT_DIRECTION_OUT, "AbsoluteTimePosition"},
	& (action_argument_t) {"RelCount", ARGUMENT_DIRECTION_OUT, "RelativeCounterPosition"},
	& (action_argument_t) {"AbsCount", ARGUMENT_DIRECTION_OUT, "AbsoluteCounterPosition"},
	NULL,
};

static action_argument_t *arguments_get_device_capabilities[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"PlayMedia", ARGUMENT_DIRECTION_OUT, "PossiblePlaybackStorageMedia"},
	& (action_argument_t) {"RecMedia", ARGUMENT_DIRECTION_OUT, "PossibleRecordStorageMedia"},
	& (action_argument_t) {"RecQualityModes", ARGUMENT_DIRECTION_OUT, "PossibleRecordQualityModes"},
	NULL,
};

static action_argument_t *arguments_get_transport_settings[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"PlayMode", ARGUMENT_DIRECTION_OUT, "CurrentPlayMode"},
	& (action_argument_t) {"RecQualityMode", ARGUMENT_DIRECTION_OUT, "CurrentRecordQualityMode"},
	NULL,
};

static action_argument_t *arguments_stop[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	NULL,
};

static action_argument_t *arguments_play[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"Speed", ARGUMENT_DIRECTION_IN, "TransportPlaySpeed"},
	NULL,
};

static action_argument_t *arguments_seek[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"Unit", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_SeekMode"},
	& (action_argument_t) {"Target", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_SeekTarget"},
	NULL,
};

static action_argument_t *arguments_next[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	NULL,
};

static action_argument_t *arguments_previous[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	NULL,
};

static service_action_t *avtransport_actions[] = {
	/* required */
	& (service_action_t) {"SetAVTransportURI", arguments_set_avtransport_uri, avtransport_set_avtransport_uri},
	& (service_action_t) {"GetMediaInfo", arguments_get_media_info, avtransport_get_media_info},
	& (service_action_t) {"GetTransportInfo", arguments_get_transport_info, avtransport_get_transport_info},
	& (service_action_t) {"GetPositionInfo", arguments_get_position_info, avtransport_get_position_info},
	& (service_action_t) {"GetDeviceCapabilities", arguments_get_device_capabilities, avtransport_get_device_capabilities},
	& (service_action_t) {"GetTransportSettings", arguments_get_transport_settings, avtransport_get_transport_settings},
	& (service_action_t) {"Stop", arguments_stop, avtransport_stop},
	& (service_action_t) {"Play", arguments_play, avtransport_play},
	& (service_action_t) {"Seek", arguments_seek, avtransport_seek},
	& (service_action_t) {"Next", arguments_next, avtransport_next},
	& (service_action_t) {"Previous", arguments_previous, avtransport_previous},
	/* optional */
	NULL,
};

/**
  */
typedef struct transport_instance_s {
	/** */
	list_t head;
	/** */
	uint32_t transportid;
	/** */
	char *currenturi;
	/** */
	char *currenturimetadata;
	/** */
	char *playspeed;
	/** */
	transport_state_t transport_state;
	/** */
	transport_status_t transport_status;
} transport_instance_t;

static int transport_instance_uninit (transport_instance_t *instance)
{
	list_del(&instance->head);
	instance_ids[instance->transportid] = 0;
	free(instance->currenturi);
	free(instance->currenturimetadata);
	free(instance->playspeed);
	free(instance);
	return 0;
}

static uint32_t transport_instance_init (uint32_t instanceid)
{
	transport_instance_t *inst;
	if (instance_ids[instanceid] == 1) {
		debugf("instanceid '%u' is in use", instanceid);
		return (uint32_t) -1;
	}
	inst = (transport_instance_t *) malloc(sizeof(transport_instance_t));
	if (inst == NULL) {
		debugf("malloc() failed");
		return (uint32_t) -1;
	}
	memset(inst, 0, sizeof(transport_instance_t));
	list_add(&inst->head, &instance_list);
	inst->transportid = instanceid;
	inst->transport_state = TRANSPORT_STATE_STOPPED;
	inst->transport_status = TRANSPORT_STATUS_OK;
	instance_ids[instanceid] = 1;
	debugf("created new transport instance '%u'", inst->transportid);
	return inst->transportid;
}

static transport_instance_t * transport_instance_get (uint32_t id)
{
	transport_instance_t *inst;
	list_for_each_entry(inst, &instance_list, head) {
		if (instance_ids[id] == 1 && inst->transportid == id) {
			return inst;
		}
	}
	debugf("no instance found with id '%u'", id);
	return NULL;
}

uint32_t transport_instance_new (device_t *device)
{
	uint32_t i;
	/* start from 1, 0 is reserved */
	for (i = 1; i < TRANSPORT_INSTANCE_MAX; i++) {
		if (instance_ids[i] == 0) {
			return transport_instance_init(i);
		}
	}
	return (uint32_t) -1;
}

int transport_instance_delete (device_t *device, uint32_t instanceid)
{
	transport_t *transport;
	transport_instance_t *inst;
	inst = transport_instance_get(instanceid);
	if (inst == NULL) {
		return -1;
	}
	inst->transport_state = TRANSPORT_STATE_STOPPED;
	inst->transport_status = TRANSPORT_STATUS_OK;

	transport = (transport_t *) device_service_find(device, TRANSPORT_SERVICE_ID);
	render_stop(transport->render);

	transport_notify_lastchange(device, instanceid);
	transport_instance_uninit(inst);
	return 0;
}

const char * transport_instance_transportstate (uint32_t instanceid)
{
	transport_instance_t *inst;
	inst = transport_instance_get(instanceid);
	if (inst == NULL) {
		return NULL;
	}
	return allowed_values_transportstate[inst->transport_state];
}

const char * transport_instance_transportstatus (uint32_t instanceid)
{
	transport_instance_t *inst;
	inst = transport_instance_get(instanceid);
	if (inst == NULL) {
		return NULL;
	}
	return allowed_values_transportstatus[inst->transport_status];
}

const char * transport_instance_currenturi (uint32_t instanceid)
{
	transport_instance_t *inst;
	inst = transport_instance_get(instanceid);
	if (inst == NULL) {
		return NULL;
	}
	return inst->currenturi;
}

const char * transport_instance_currenturimetadata (uint32_t instanceid)
{
	transport_instance_t *inst;
	inst = transport_instance_get(instanceid);
	if (inst == NULL) {
		return NULL;
	}
	return inst->currenturimetadata;
}

const char * transport_instance_playspeed (uint32_t instanceid)
{
	transport_instance_t *inst;
	inst = transport_instance_get(instanceid);
	if (inst == NULL) {
		return NULL;
	}
	return inst->playspeed;
}

static int transport_notify_lastchange (device_t *device, uint32_t instanceid)
{
	char *playspeed;
	char *currenturi;
	char *transportstate;
	char *transportstatus;
	char *currenturimetadata;
	const char *varnames[] = {
		"LastChange",
		NULL
	};
	char *varvalues[] = {
		NULL,
		NULL
	};
	debugf("transport nofity last change");
	playspeed = xml_escape(transport_instance_playspeed(instanceid), 1);
	currenturi = xml_escape(transport_instance_currenturi(instanceid), 1);
	transportstate = xml_escape(transport_instance_transportstate(instanceid), 1);
	transportstatus = xml_escape(transport_instance_transportstatus(instanceid), 1);
	currenturimetadata = xml_escape(transport_instance_currenturimetadata(instanceid), 1);
	asprintf(&varvalues[0],
			"<Event xmlns = \"urn:schemas-upnp-org:metadata-1-0/AVT/\">"
			"  <InstanceID val=\"%u\">"
		        "    <TransportState val=\"%s\"/>"
		        "    <TransportStatus val=\"%s\"/>"
		        "    <AVTransportURI val=\"%s\"/>"
			"    <CurrentTrackMetaData val=\"%s\"/>"
		        "    <TransportPlaySpeed val=\"%s\"/>"
		        "    <CurrentTransportActions val=\"Play,Stop,Pause,Seek,Next,Previous\"/>"
		        "  </InstanceID>"
		        "</Event>",
		        instanceid,
		        transportstate,
		        transportstatus,
		        currenturi,
		        currenturimetadata,
		        playspeed);
	UpnpNotify(device->handle, device->uuid, TRANSPORT_SERVICE_ID, (const char **) varnames, (const char **) varvalues, 1);
	free(varvalues[0]);
	return 0;
}

static int avtransport_set_avtransport_uri (device_service_t *service, upnp_event_action_t *request)
{
	char *currenturi;
	char *currenturimetadata;
	uint32_t instanceid;
	transport_instance_t *tinstance;

	instanceid = xml_get_ui4(request->request, "InstanceID");
	currenturi = xml_get_string(request->request, "CurrentURI");
	currenturimetadata = xml_get_string(request->request, "CurrentURIMetaData");

	debugf("avtransport_set_avtransport_uri;\n"
	       "  instanceid        : %u\n"
	       "  currenturi        : %s\n"
	       "  currenturimetadata: %s\n",
	       instanceid,
	       currenturi,
	       currenturimetadata);
	tinstance = transport_instance_get(instanceid);
	if (tinstance == NULL) {
		request->errcode = 718;
		return -1;
	}
	if (tinstance->currenturi != NULL) {
		debugf("argh, we do have a current uri already!");
		free(tinstance->currenturi);
	}
	tinstance->currenturi = strdup(currenturi);
	if (tinstance->currenturi == NULL) {
		debugf("strdup(currenturi) failed");
		request->errcode = 718;
		return -1;
	}
	tinstance->currenturimetadata = (currenturimetadata) ? strdup(currenturimetadata) : NULL;

	return 0;
}

static int avtransport_get_media_info (device_service_t *service, upnp_event_action_t *request)
{
	debugf("avtransport_get_media_info");
	assert(0);
	return 0;
}

static int avtransport_get_transport_info (device_service_t *service, upnp_event_action_t *request)
{
	uint32_t instanceid;
	transport_instance_t *tinstance;

	instanceid = xml_get_ui4(request->request, "InstanceID");
	tinstance = transport_instance_get(instanceid);
	debugf("avtransport_get_transport_info;\n"
	       "instanceid: %u\n",
	       instanceid);
	if (tinstance == NULL) {
		request->errcode = 718;
		return -1;
	}

	upnp_add_response(request, service->type, "CurrentTransportState", allowed_values_transportstate[tinstance->transport_state]);
	upnp_add_response(request, service->type, "CurrentTransportStatus", allowed_values_transportstatus[tinstance->transport_status]);
	upnp_add_response(request, service->type, "CurrentSpeed", tinstance->playspeed);

	return 0;
}

static int avtransport_get_position_info (device_service_t *service, upnp_event_action_t *request)
{
	char *position;
	char *duration;
	uint32_t instanceid;
	transport_t *transport;
	render_info_t renderinfo;
	transport_instance_t *tinstance;

	instanceid = xml_get_ui4(request->request, "InstanceID");
	debugf("avtransport_get_position_info;\n"
		"instanceid: %u\n",
		instanceid);

	tinstance = transport_instance_get(instanceid);
	if (tinstance == NULL) {
		request->errcode = 718;
		return -1;
	}

	transport = (transport_t *) service;
	position = NULL;
	duration = NULL;
	if (render_info(transport->render, &renderinfo) == 0) {
		asprintf(&position, "%lu:%02lu:%02lu",
				(renderinfo.position != (unsigned long) -1) ? (renderinfo.position / (60 * 60)) : 99,
				(renderinfo.position != (unsigned long) -1) ? ((renderinfo.position / 60) % 60) : 99,
				(renderinfo.position != (unsigned long) -1) ? (renderinfo.position % 60) : 99);
		asprintf(&duration, "%lu:%02lu:%02lu",
				(renderinfo.duration != (unsigned long) -1) ? (renderinfo.duration / (60 * 60)) : 99,
				(renderinfo.duration != (unsigned long) -1) ? ((renderinfo.duration / 60) % 60) : 99,
				(renderinfo.duration != (unsigned long) -1) ? (renderinfo.duration % 60) : 99);
	}

	debugf("render state: %u, position: %s / %s", renderinfo.state, position, duration);

	upnp_add_response(request, service->type, "Track", "0");
	upnp_add_response(request, service->type, "TrackDuration", (duration) ? duration : "NOT_IMPLEMENTED");
	upnp_add_response(request, service->type, "TrackMetaData", tinstance->currenturimetadata);
	upnp_add_response(request, service->type, "TrackURI", tinstance->currenturi);
	upnp_add_response(request, service->type, "RelTime", (position) ? position : "NOT_IMPLEMENTED");
	upnp_add_response(request, service->type, "AbsTime", "NOT_IMPLEMENTED");
	upnp_add_response(request, service->type, "RelCount", "0");
	upnp_add_response(request, service->type, "AbsCount", "0");

	free(position);
	free(duration);

	if (renderinfo.state == RENDER_STATE_STOPPED) {
		debugf("stopped playing, change internal stats and event as usual");
		tinstance->transport_state = TRANSPORT_STATE_STOPPED;
		tinstance->transport_status = TRANSPORT_STATUS_OK;
		transport_notify_lastchange(service->device, instanceid);
	}

	return 0;
}

static int avtransport_get_device_capabilities (device_service_t *service, upnp_event_action_t *request)
{
	debugf("avtransport_get_device_capabilities");
	assert(0);
	return 0;
}

static int avtransport_get_transport_settings (device_service_t *service, upnp_event_action_t *request)
{
	debugf("avtransport_get_transport_settings");
	assert(0);
	return 0;
}

static int avtransport_stop (device_service_t *service, upnp_event_action_t *request)
{
	char *playspeed;
	uint32_t instanceid;
	transport_t *transport;
	transport_instance_t *tinstance;

	instanceid = xml_get_ui4(request->request, "InstanceID");
	playspeed = xml_get_string(request->request, "Speed");

	debugf("avtransport_stop;\n"
	       "  instanceid: %u\n",
	       instanceid)

	tinstance = transport_instance_get(instanceid);
	if (tinstance == NULL) {
		request->errcode = 718;
		return 0;
	}
	tinstance->transport_state = TRANSPORT_STATE_STOPPED;
	tinstance->transport_status = TRANSPORT_STATUS_OK;

	transport = (transport_t *) service;
	render_stop(transport->render);

	transport_notify_lastchange(service->device, instanceid);

	return 0;
}

static int avtransport_play (device_service_t *service, upnp_event_action_t *request)
{
	char *playspeed;
	uint32_t instanceid;
	transport_t *transport;
	transport_instance_t *tinstance;

	instanceid = xml_get_ui4(request->request, "InstanceID");
	playspeed = xml_get_string(request->request, "Speed");

	debugf("avtransport_play;\n"
	       "  instanceid: %u\n"
	       "  playspeed: %s\n",
	       instanceid,
	       playspeed);

	tinstance = transport_instance_get(instanceid);
	if (tinstance == NULL) {
		return 0;
	}

	if (tinstance->currenturi == NULL) {
		request->errcode = 718;
		return -1;
	}

	if (tinstance->playspeed != NULL) {
		free(tinstance->playspeed);
	}
	tinstance->playspeed = strdup(playspeed);

	transport = (transport_t *) service;

	if (tinstance->transport_state == TRANSPORT_STATE_STOPPED) {
		if (render_play(transport->render, tinstance->currenturi) == 0) {
			tinstance->transport_state = TRANSPORT_STATE_PLAYING;
			tinstance->transport_status = TRANSPORT_STATUS_OK;
		} else {
			debugf("mediarender_play(%s) failed", tinstance->currenturi);
		}
	}

	transport_notify_lastchange(service->device, instanceid);

	return 0;
}

static int avtransport_seek (device_service_t *service, upnp_event_action_t *request)
{
	debugf("avtransport_seek");
	assert(0);
	return 0;
}

static int avtransport_next (device_service_t *service, upnp_event_action_t *request)
{
	debugf("avtransport_next");
	assert(0);
	return 0;
}

static int avtransport_previous (device_service_t *service, upnp_event_action_t *request)
{
	debugf("avtransport_previous");
	assert(0);
	return 0;
}

int avtransport_uninit (device_service_t *transport)
{
	int i;
	transport_instance_t *next;
	transport_instance_t *inst;
	service_variable_t *variable;
	debugf("uninitializing av transport manager service");
	list_for_each_entry_safe(inst, next, &instance_list, head) {
		debugf("uninitializing transport instance '%u'", inst->transportid);
		transport_instance_uninit(inst);
	}
	for (i = 0; (variable = transport->variables[i]) != NULL; i++) {
		free(variable->value);
	}
	free(transport);
	return 0;
}

device_service_t * avtransport_init (render_t *render)
{
	int i;
	transport_t *transport;
	service_variable_t *variable;
	debugf("initializing av transport service struct");
	transport = (transport_t *) malloc(sizeof(transport_t));
	if (transport == NULL) {
		debugf("malloc(sizeof(transport_t)) failed");
		goto out;
	}
	memset(transport, 0, sizeof(transport_t));
	transport->service.name = TRANSPORT_SERVICE_NAME;
	transport->service.id = TRANSPORT_SERVICE_ID;
	transport->service.type = TRANSPORT_SERVICE_TYPE;
	transport->service.scpdurl = "/upnp/avtransport.xml";
	transport->service.eventurl = "/upnp/event/avtransport1";
	transport->service.controlurl = "/upnp/control/avtransport1";
	transport->service.actions = avtransport_actions;
	transport->service.variables = avtransport_variables;
	transport->service.uninit = avtransport_uninit;

	debugf("initializing av transport service");
	if (service_init(&transport->service) != 0) {
		debugf("service_init(&transport->service) failed");
		free(transport);
		transport = NULL;
		goto out;
	}

	variable = service_variable_find(&transport->service, "CurrentTransportActions");
	if (variable != NULL) {
		variable->value = strdup("");
	}
	variable = service_variable_find(&transport->service, "TransportState");
	if (variable != NULL) {
		variable->value = strdup("");
	}
	variable = service_variable_find(&transport->service, "Event");
	if (variable != NULL) {
		variable->value = strdup("");
	}
	variable = service_variable_find(&transport->service, "LastChange");
	if (variable != NULL) {
		variable->value = xml_escape("<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\"/>", 0);
	}

	debugf("initializing ac transport instances");
	for (i = 0; i < TRANSPORT_INSTANCE_MAX; i++) {
		instance_ids[i] = 0;
	}
	debugf("initializing instance list");
	list_init(&instance_list);

	transport->render = render;

#if defined(ENABLE_OPTIONAL)
#else
	debugf("creating reserved '0' instance");
	transport_instance_init(0);
#endif

	debugf("initialized av transport service");
out:	return &transport->service;
}
