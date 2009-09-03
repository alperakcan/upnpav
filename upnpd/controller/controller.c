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
#include "upnpd.h"
#include "common.h"

#include "controller.h"

extern controller_gui_t controller_console;

static controller_gui_t *controller_gui[] = {
	&controller_console,
	NULL,
};

typedef enum {
	OPT_GUI    = 0,
} mediaserver_options_t;

static char *mediaserver_options[] = {
	[OPT_GUI]    = "gui",
	NULL,
};

static int controller_main (char *options)
{
	controller_gui_t **g;

	int err;
	char *str;
	char *gui;
	char *value;
	char *suboptions;

	err = 0;
	gui = NULL;
	str = strdup(options);
	if (str == NULL) {
		debugf("strdup(options); failed");
		return -1;
	}
	suboptions = str;
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, mediaserver_options, &value)) {
			case OPT_GUI:
				if (value == NULL) {
					debugf("value is missing for gui option");
					err = 1;
					continue;
				}
				gui = value;
				break;
		}
	}

	if (gui == NULL) {
		gui = "console";
	}

	debugf("starting controller;\n"
	       "\tgui   : %s\n",
	       (gui) ? gui : "null");

	for (g = controller_gui; *g; g++) {
		if (strcmp((*g)->name, gui) == 0) {
			free(str);
			return (*g)->main(options);
		}
	}

	debugf("no gui found '%s'", gui);
	free(str);
	return -1;
}

upnpd_application_t controller = {
	.name = "controller",
	.description = "controller point device",
	.main = &controller_main,
};
