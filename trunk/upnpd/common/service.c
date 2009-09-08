/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 CoreCodec, Inc., http://www.CoreCodec.com
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
#include <unistd.h>
#include <inttypes.h>

#include "platform.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

int service_init (device_service_t *service)
{
	int ret;
	ret = -1;
	debugf("initializing service '%s'", service->name);
	service->mutex = thread_mutex_init("service->mutex", 0);
	if (service->mutex == NULL) {
		debugf("thread_mutex_init(service->mutex, 0) failed");
		goto out;
	}
	debugf("generating service '%s' description", service->name);
	service->description = description_generate_from_service(service);
	if (service->description == NULL) {
		debugf("description_generate_from_service(service) failed");
		thread_mutex_destroy(service->mutex);
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
	thread_mutex_destroy(service->mutex);
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
