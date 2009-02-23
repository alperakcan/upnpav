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

/**
  */
typedef struct rendering_s {
	/** */
	device_service_t service;
} rendering_t;

static int renderingcontrol_list_presets (device_service_t *service, struct Upnp_Action_Request *request)
{
	debugf("renderingcontrol_list_presets");
	assert(0);
	return 0;
}

static int renderingcontrol_set_preset (device_service_t *service, struct Upnp_Action_Request *request)
{
	debugf("renderingcontrol_set_preset");
	assert(0);
	return 0;
}

static service_variable_t *renderingcontrol_variables[] = {
	/* required */
	& (service_variable_t) {"LastChange", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_YES, NULL, NULL},
	& (service_variable_t) {"PresetNameList", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Channel", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_InstanceID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_PresetName", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	/* optional */
	NULL,
};

static action_argument_t *arguments_list_presets[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"CurrentPresetNameList", ARGUMENT_DIRECTION_OUT, "PresetNameList"},
	NULL,
};

static action_argument_t *arguments_select_preset[] = {
	& (action_argument_t) {"InstanceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_InstanceID"},
	& (action_argument_t) {"PresetName", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_PresetName"},
	NULL,
};

static service_action_t *renderingcontrol_actions[] = {
	/* required */
	& (service_action_t) {"ListPresets", arguments_list_presets, renderingcontrol_list_presets},
	& (service_action_t) {"SelectPreset", arguments_select_preset, renderingcontrol_set_preset},
	/* optional */
	NULL,
};

int renderingcontrol_uninit (device_service_t *rendering)
{
	int i;
	service_variable_t *variable;
	debugf("uninitializing rendering control service");
	for (i = 0; (variable = rendering->variables[i]) != NULL; i++) {
		free(variable->value);
	}
	free(rendering);
	return 0;
}

device_service_t * renderingcontrol_init (void)
{
	rendering_t *rendering;
	service_variable_t *variable;
	debugf("initializing rendering control service struct");
	rendering = (rendering_t *) malloc(sizeof(rendering_t));
	if (rendering == NULL) {
		debugf("malloc(sizeof(rendering_t)) failed");
		goto out;
	}
	memset(rendering, 0, sizeof(rendering_t));
	rendering->service.name = "renderingcontrol";
	rendering->service.id = "urn:upnp-org:serviceId:RenderingControl";
	rendering->service.type = "urn:schemas-upnp-org:service:RenderingControl:1";
	rendering->service.scpdurl = "/upnp/renderingcontrol.xml";
	rendering->service.eventurl = "/upnp/event/renderingcontrol1";
	rendering->service.controlurl = "/upnp/control/renderingcontrol1";
	rendering->service.actions = renderingcontrol_actions;
	rendering->service.variables = renderingcontrol_variables;
	rendering->service.uninit = renderingcontrol_uninit;
	
	debugf("initializing av rendering service");
	if (service_init(&rendering->service) != 0) {
		debugf("service_init(&rendering->service) failed");
		free(rendering);
		rendering = NULL;
		goto out;
	}

	variable = service_variable_find(&rendering->service, "LastChange");
	if (variable != NULL) {
		variable->value = xml_escape("<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\"/>", 0);
	}

	debugf("initialized rendering control service");
out:	return &rendering->service;
}
