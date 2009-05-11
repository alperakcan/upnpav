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
#include <ctype.h>
#include <inttypes.h>
#include <pthread.h>

#if HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "gena.h"
#include "upnp.h"
#include "upnpd.h"
#include "common.h"

#include "controller.h"

static int browse_device (client_t *client, char *device, char *object)
{
	entry_t *entry;
	entry = controller_browse_children(client, device, object);
	if (entry != NULL) {
		entry_print(entry);
		entry_uninit(entry);
	}
	return 0;
}

static int metadata_device (client_t *client, char *device, char *object)
{
	entry_t *entry;
	entry = controller_browse_metadata(client, device, object);
	if (entry != NULL) {
		entry_dump(entry);
		entry_uninit(entry);
	}
	return 0;
}

static int refresh_devices (client_t *client)
{
	return client_refresh(client, 1);
}

static int list_devices (client_t *client)
{
	client_device_t *device;
	pthread_mutex_lock(&client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		printf("-- %s\n", device->name);
		printf("  type         : %s\n", device->type);
		printf("  uuid         : %s\n", device->uuid);
		printf("  expiretime   : %d\n", device->expiretime);
	}
	pthread_mutex_unlock(&client->mutex);
	return 0;
}

static int print_device (client_t *client, char *name)
{
	client_device_t *device;
	client_service_t *service;
	client_variable_t *variable;
	pthread_mutex_lock(&client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		if (strcmp(name, device->name) == 0) {
			printf("-- %s\n", device->name);
			printf("  type         : %s\n", device->type);
			printf("  uuid         : %s\n", device->uuid);
			printf("  expiretime   : %d\n", device->expiretime);
			list_for_each_entry(service, &device->services, head) {
				printf("  -- type       : %s\n", service->type);
				printf("     id         : %s\n", service->id);
				printf("     sid        : %s\n", service->sid);
				printf("     controlurl : %s\n", service->controlurl);
				printf("     eventurl   : %s\n", service->eventurl);
				list_for_each_entry(variable, &service->variables, head) {
					printf("    -- name  : %s\n", variable->name);
					printf("       value : %s\n", variable->value);
				}
			}
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return 0;
}

static int print_devices (client_t *client)
{
	client_device_t *device;
	client_service_t *service;
	client_variable_t *variable;
	pthread_mutex_lock(&client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		printf("-- %s\n", device->name);
		printf("  type         : %s\n", device->type);
		printf("  uuid         : %s\n", device->uuid);
		printf("  expiretime   : %d\n", device->expiretime);
		list_for_each_entry(service, &device->services, head) {
			printf("  -- type       : %s\n", service->type);
			printf("     id         : %s\n", service->id);
			printf("     sid        : %s\n", service->sid);
			printf("     controlurl : %s\n", service->controlurl);
			printf("     eventurl   : %s\n", service->eventurl);
			list_for_each_entry(variable, &service->variables, head) {
				printf("    -- name  : %s\n", variable->name);
				printf("       value : %s\n", variable->value);
			}
		}
	}
	pthread_mutex_unlock(&client->mutex);
	return 0;
}

static int controller_rline_process (client_t *device, char *command)
{
	int ret;
	char *b;
	char *p;
	int argc;
	char **argv;

	ret = 0;
	argc = 0;
	argv = NULL;
	b = strdup(command);
	p = b;

	while (*p) {
		while (isspace (*p)) {
			p++;
		}

		if (*p == '"' || *p == '\'') {
			char const delim = *p;
			char *const begin = ++p;

			while (*p && *p != delim) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else {
				goto out;
			}
		} else {
			char *const begin = p;

			while (*p && ! isspace (*p)) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else if (p != begin) {
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			}
		}
	}

	if (strcmp(argv[0], "help") == 0) {
		printf("help                                - this text\n"
		       "quit                                - quit controller\n"
		       "refresh                             - refresh device list\n"
		       "list                                - list all devices\n"
		       "print [device]                      - print all devices, or device\n"
		       "browse <objectid>                   - browse a device\n"
		       "metadata <objectid>                 - print metadata information of an object\n"
		       "play <objectid> <server> <renderer> - play an mediaserver object on a mediarender\n");
	} else if (strcmp(argv[0], "quit") == 0) {
		ret = -1;
	} else if (strcmp(argv[0], "refresh") == 0) {
		refresh_devices(device);
	} else if (strcmp(argv[0], "list") == 0) {
		list_devices(device);
	} else if (strcmp(argv[0], "print") == 0) {
		if (argc == 1) {
			print_devices(device);
		} else if (argc == 2) {
			print_device(device, argv[1]);
		}
	} else if (argc == 3 && strcmp(argv[0], "browse") == 0) {
		browse_device(device, argv[1], argv[2]);
	} else if (argc == 3 && strcmp(argv[0], "metadata") == 0) {
		metadata_device(device, argv[1], argv[2]);
	}

out:	free(argv);
	free(b);
	return ret;
}

static char * controller_rline_strip (char *buf)
{
	char *start;

	/* Strip off trailing whitespace, returns, etc */
	while ((*buf != '\0') && (buf[strlen(buf) - 1] < 33)) {
		buf[strlen(buf) - 1] = '\0';
	}
	start = buf;

	/* Strip off leading whitespace, returns, etc */
	while (*start && (*start < 33)) {
		start++;
	}

	return start;
}

static int console_main (char *options)
{
	int rc;
	int ret;
	int running;
	char *rline;
	char *rcommand;
	client_t *controller;

	ret = -1;

	controller = controller_init(options);
	if (controller == NULL) {
		debugf("controller_init() failed");
		goto out;
	}

	running = 1;
	while (running) {
		rline = readline("console> ");
		rcommand = controller_rline_strip(rline);
		if (rcommand && *rcommand) {
			add_history(rcommand);
			if (controller_rline_process(controller, rcommand) != 0) {
				running = 0;
			}
		}
		free(rline);
	};

	rc = controller_uninit(controller);
	if (rc != 0) {
		debugf("controller_uninit(controller) failed");
		goto out;
	}
	ret = 0;
out:	return ret;
}

controller_gui_t controller_console = {
	.name = "console",
	.main = console_main,
};
