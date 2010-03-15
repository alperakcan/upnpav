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

#include "upnpd.h"

#include "controller.h"
#include "platform.h"

typedef enum {
	OPT_INTERFACE = 0,
} controller_options_t;

static char *controller_options[] = {
	"interface",
	NULL,
};

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

static int browse_local (const char *path)
{
	upnpd_item_t *item;
	upnpd_item_t *items;
	items = upnpd_controller_browse_local(path);
	for (item = items; item; item = item->next) {
		printf("%s - %s (pid: %s, class: %s, size: %llu)\n", item->id, item->title, item->pid, item->class, item->size);
	}
	upnpd_controller_free_items(items);
	return 0;
}

static int browse_device (upnpd_controller_t *controller, char *device, char *object)
{
	upnpd_item_t *item;
	upnpd_item_t *items;
	items = upnpd_controller_browse_device(controller, device, object);
	for (item = items; item; item = item->next) {
		printf("%s - %s (pid: %s, class: %s, size: %llu)\n", item->id, item->title, item->pid, item->class, item->size);
	}
	upnpd_controller_free_items(items);
	return 0;
}

static int metadata_device (upnpd_controller_t *controller, char *device, char *object)
{
	upnpd_item_t *item;
	item = upnpd_controller_metadata_device(controller, device, object);
	if (item != NULL) {
		printf("id      : %s\n", item->id);
		printf("pid     : %s\n", item->pid);
		printf("title   : %s\n", item->title);
		printf("class   : %s\n", item->class);
		printf("size    : %llu\n", item->size);
		printf("location: %s\n", item->location);
	}
	upnpd_controller_free_items(item);
	return 0;
}

static int refresh_devices (upnpd_controller_t *controller)
{
	return upnpd_controller_scan_devices(controller, 1);
}

static int list_devices (upnpd_controller_t *controller)
{
	upnpd_device_t *device;
	upnpd_device_t *devices;
	devices = upnpd_controller_get_devices(controller);
	for (device = devices; device; device = device->next) {
		printf("-- %s\n", device->name);
		printf("  type       : %s\n", device->type);
		printf("  uuid       : %s\n", device->uuid);
		printf("  location   : %s\n", device->location);
		printf("  expiretime : %d\n", device->expiretime);
	}
	upnpd_controller_free_devices(devices);
	return 0;
}

static int controller_rline_process (upnpd_controller_t *controller, char *command)
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
		       "browse <objectid>                   - browse a device\n"
		       "metadata <objectid>                 - print metadata information of an object\n");
	} else if (strcmp(argv[0], "quit") == 0) {
		ret = -1;
	} else if (strcmp(argv[0], "refresh") == 0) {
		refresh_devices(controller);
	} else if (strcmp(argv[0], "list") == 0) {
		list_devices(controller);
	} else if (argc == 3 && strcmp(argv[0], "browse") == 0) {
		browse_device(controller, argv[1], argv[2]);
	} else if (argc == 2 && strcmp(argv[0], "local") == 0) {
		browse_local(argv[1]);
	} else if (argc == 3 && strcmp(argv[0], "metadata") == 0) {
		metadata_device(controller, argv[1], argv[2]);
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

static int controller_main (char *options)
{
	int rc;
	int err;
	int ret;
	int running;
	char *rline;
	char *value;
	char *rcommand;
	char *interface;
	char *suboptions;
	upnpd_controller_t *controller;

	ret = -1;
	err = 0;
	interface = NULL;

	suboptions = options;
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, controller_options, &value)) {
			case OPT_INTERFACE:
				if (value == NULL) {
					printf("value is missing for interface option\n");
					err = 1;
					continue;
				}
				interface = value;
				break;
		}
	}

	controller = upnpd_controller_init(interface);
	if (controller == NULL) {
		printf("controller_init() failed\n");
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

	rc = upnpd_controller_uninit(controller);
	if (rc != 0) {
		printf("controller_uninit(controller) failed\n");
		goto out;
	}
	ret = 0;
out:	return ret;
}

upnpd_application_t controller = {
	.name = "controller",
	.description = "upnp av controller device",
	.main = &controller_main,
};
