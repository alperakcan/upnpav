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
#include <unistd.h>
#include <inttypes.h>

#include "platform.h"
#include "database.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

/**
  */
typedef struct registrar_s {
	/** */
	device_service_t service;
} registrar_t;

static service_variable_t *registrar_variables[] = {
	/* required */
	& (service_variable_t) {"A_ARG_TYPE_DeviceID", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_RegistrationReqMsg", VARIABLE_DATATYPE_BIN_BASE64, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_RegistrationRespMsg", VARIABLE_DATATYPE_BIN_BASE64, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Result", VARIABLE_DATATYPE_I4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"AuthorizationDeniedUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"AuthorizationGrantedUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"ValidationRevokedUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"ValidationSucceededUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	NULL,
};

static int registrar_is_authorized (device_service_t *service, upnp_event_action_t *request)
{
	int rc;
	debugf("registrar is authorized");
	rc = upnp_add_response(request, service->type, "Result", "1");
	return rc;
}

static action_argument_t *arguments_is_authorized[] = {
	& (action_argument_t) {"DeviceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_DeviceID"},
	& (action_argument_t) {"Result", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_Result"},
	NULL,
};

static service_action_t *registrar_actions[] = {
	& (service_action_t) {"IsAuthorized", arguments_is_authorized, registrar_is_authorized},
	NULL,
};

static int registrar_uninit (device_service_t *registrar)
{
	int i;
	service_variable_t *variable;
	debugf("registrar uninit");
	for (i = 0; (variable = registrar->variables[i]) != NULL; i++) {
		free(variable->value);
	}
	free(registrar);
	return 0;
}

device_service_t * registrar_init (void)
{
	registrar_t *registrar;
	service_variable_t *variable;
	registrar = NULL;
	debugf("initializing content directory service struct");
	registrar = (registrar_t *) malloc(sizeof(registrar_t));
	if (registrar == NULL) {
		debugf("malloc(sizeof(registrar_t)) failed");
		goto out;
	}
	memset(registrar, 0, sizeof(registrar_t));
	registrar->service.name = "registrar";
	registrar->service.id = "urn:microsoft.com:serviceId:X_MS_MediaReceiverRegistrar";
	registrar->service.type = "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1";
	registrar->service.scpdurl = "/upnp/registrar.xml";
	registrar->service.eventurl = "/upnp/event/registrar1";
	registrar->service.controlurl = "/upnp/control/registrar1";
	registrar->service.actions = registrar_actions;
	registrar->service.variables = registrar_variables;
	registrar->service.vfscallbacks = NULL;
	registrar->service.uninit = registrar_uninit;

	debugf("initializing content directory service");
	if (service_init(&registrar->service) != 0) {
		debugf("service_init(&registrar->service) failed");
		free(registrar);
		registrar = NULL;
		goto out;
	}
	variable = service_variable_find(&registrar->service, "SystemUpdateID");
	if (variable != NULL) {
		variable->value = strdup("0");
	}

	debugf("initialized content directory service");
out:	return &registrar->service;
}

