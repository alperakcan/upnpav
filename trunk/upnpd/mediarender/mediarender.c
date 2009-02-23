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
#include <readline/readline.h>
#include <readline/history.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "upnpd.h"
#include "common.h"

static int mediarender_rline_process (device_t *device, char *command)
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
		printf("help - this text\n"
		       "quit - quit mediarender\n");
	} else if (strcmp(argv[0], "quit") == 0) {
		ret = -1;
	}

out:
	free(argv);
	free(b);
	return ret;
}

static char * mediarender_rline_strip (char *buf)
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

int mediarender_main (char *options)
{
	int rc;
	int ret;
	int running;
	char *rline;
	char *rcommand;
	device_t *mediarender;

	ret = -1;

	mediarender = mediarender_init(options);
	if (mediarender == NULL) {
		debugf("mediarender_init() failed");
		goto out;
	}

	running = 1;
	while (running) {
		rline = readline("console> ");
		rcommand = mediarender_rline_strip(rline);
		if (rcommand && *rcommand) {
			add_history(rcommand);
			if (mediarender_rline_process(mediarender, rcommand) != 0) {
				running = 0;
			}
		}
		free(rline);
	};

	rc = mediarender_uninit(mediarender);
	if (rc != 0) {
		debugf("mediarender_uninit(mediarender) failed");
		goto out;
	}
	ret = 0;
out:	return ret;
}

upnpd_application_t mediarender = {
	.name = "mediarender",
	.description = "upnp av mediarender device",
	.main = &mediarender_main,
};
