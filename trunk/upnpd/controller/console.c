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
#include <ctype.h>
#include <inttypes.h>

#if HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "upnpd.h"
#include "common.h"

#include "controller.h"

#if !HAVE_LIBREADLINE
static int rinit = 0;
static list_t rlist;

static char * readline (const char *shell)
{
	char *buf;
	size_t size;
	size_t read;
	if (rinit == 0) {
		list_init(&rlist);
		rinit = 1;
	}
	printf("%s ", shell);
	buf = NULL;
	size = 0;
	read = getdelim(&buf, &size, '\n', stdin);
	if (read == -1) {
		return NULL;
	}
	buf[read - 1] = '\0';
	return buf;
}

static int add_history (const char *line)
{
	if (rinit == 0) {
		list_init(&rlist);
		rinit = 1;
	}
	return 0;
}
#endif

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
	thread_mutex_lock(client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		printf("-- %s\n", device->name);
		printf("  type       : %s\n", device->type);
		printf("  uuid       : %s\n", device->uuid);
		printf("  location   : %s\n", device->location);
		printf("  expiretime : %d\n", device->expiretime);
	}
	thread_mutex_unlock(client->mutex);
	return 0;
}

static int print_device (client_t *client, char *name)
{
	client_device_t *device;
	client_service_t *service;
	client_variable_t *variable;
	thread_mutex_lock(client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		if (strcmp(name, device->name) == 0) {
			printf("-- %s\n", device->name);
			printf("  type       : %s\n", device->type);
			printf("  uuid       : %s\n", device->uuid);
			printf("  location   : %s\n", device->location);
			printf("  expiretime : %d\n", device->expiretime);
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
	thread_mutex_unlock(client->mutex);
	return 0;
}

static int print_devices (client_t *client)
{
	client_device_t *device;
	client_service_t *service;
	client_variable_t *variable;
	thread_mutex_lock(client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		printf("-- %s\n", device->name);
		printf("  type       : %s\n", device->type);
		printf("  uuid       : %s\n", device->uuid);
		printf("  location   : %s\n", device->location);
		printf("  expiretime : %d\n", device->expiretime);
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
	thread_mutex_unlock(client->mutex);
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
