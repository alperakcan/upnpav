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

static service_variable_t registrar_variables[] = {
	/* required */
	{"A_ARG_TYPE_DeviceID", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"A_ARG_TYPE_RegistrationReqMsg", VARIABLE_DATATYPE_BIN_BASE64, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"A_ARG_TYPE_RegistrationRespMsg", VARIABLE_DATATYPE_BIN_BASE64, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"A_ARG_TYPE_Result", VARIABLE_DATATYPE_I4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"AuthorizationDeniedUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"AuthorizationGrantedUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"ValidationRevokedUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{"ValidationSucceededUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	{ NULL },
};

static int registrar_is_authorized (device_service_t *service, upnp_event_action_t *request)
{
	int rc;
	debugf(_DBG, "registrar is authorized");
	rc = upnpd_upnp_add_response(request, service->type, "Result", "1");
	return rc;
}

static action_argument_t arguments_is_authorized[] = {
	{"DeviceID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_DeviceID"},
	{"Result", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_Result"},
	{ NULL },
};

static service_action_t registrar_actions[] = {
	{"IsAuthorized", arguments_is_authorized, registrar_is_authorized},
	{ NULL },
};

static int registrar_uninit (device_service_t *registrar)
{
	int i;
	service_variable_t *variable;
	debugf(_DBG, "registrar uninit");
	for (i = 0; (variable = &registrar->variables[i])->name != NULL; i++) {
		free(variable->value);
	}
	free(registrar);
	return 0;
}

device_service_t * upnpd_registrar_init (void)
{
	registrar_t *registrar;
	service_variable_t *variable;
	registrar = NULL;
	debugf(_DBG, "initializing content directory service struct");
	registrar = (registrar_t *) malloc(sizeof(registrar_t));
	if (registrar == NULL) {
		debugf(_DBG, "malloc(sizeof(registrar_t)) failed");
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

	debugf(_DBG, "initializing content directory service");
	if (upnpd_service_init(&registrar->service) != 0) {
		debugf(_DBG, "upnpd_service_init(&registrar->service) failed");
		free(registrar);
		registrar = NULL;
		goto out;
	}
	variable = upnpd_service_variable_find(&registrar->service, "SystemUpdateID");
	if (variable != NULL) {
		variable->value = strdup("0");
	}

	debugf(_DBG, "initialized content directory service");
out:	return &registrar->service;
}

