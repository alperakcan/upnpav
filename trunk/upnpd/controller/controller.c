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
#include <pthread.h>
#include <inttypes.h>

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
