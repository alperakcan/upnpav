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

int service_init (device_service_t *service)
{
	int ret;
	ret = -1;
	debugf("initializing service '%s'", service->name);
	if (pthread_mutex_init(&service->mutex, NULL) != 0) {
		debugf("pthread_mutex_init(&service->mutex, NULL) failed");
		goto out;
	}
	debugf("generating service '%s' description", service->name);
	service->description = description_generate_from_service(service);
	if (service->description == NULL) {
		debugf("description_generate_from_service(service) failed");
		pthread_mutex_destroy(&service->mutex);
		goto out;
	}
	ret = 0;
	debugf("initialized service\n"
	       "  name       : %s\n"
	       "  description: %s\n",
	       service->name,
	       service->description);
out:	return ret;
}

int service_uninit (device_service_t *service)
{
	pthread_mutex_destroy(&service->mutex);
	free(service->description);
	return service->uninit(service);
}

service_variable_t * service_variable_find (device_service_t *service, char *name)
{
	int i;
	service_variable_t *variable;
	for (i = 0; (variable = service->variables[i]) != NULL; i++) {
		if (strcmp(variable->name, name) == 0) {
			return variable;
		}
	}
	return NULL;
}

service_action_t * service_action_find (device_service_t *service, char *name)
{
	int i;
	service_action_t *action;
	for (i = 0; (action = service->actions[i]) != NULL; i++) {
		if (strcmp(action->name, name) == 0) {
			return action;
		}
	}
	return NULL;
}
