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

#if HAVE_LIBREADLINE
static int mediaserver_rline_process (device_t *device, char *command)
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
		printf("help    - this text\n"
		       "refresh - refresh entries\n"
		       "quit    - quit mediaserver\n");
	} else if (strcmp(argv[0], "refresh") == 0) {
		return upnpd_mediaserver_refresh(device);
	} else if (strcmp(argv[0], "quit") == 0) {
		ret = -1;
	}

out:
	free(argv);
	free(b);
	return ret;
}

static char * mediaserver_rline_strip (char *buf)
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
#endif

int mediaserver_main (char *options)
{
	int rc;
	int ret;
	int running;
	char *rline;
	char *rcommand;
	device_t *mediaserver;

	ret = -1;

	mediaserver = upnpd_mediaserver_init(options);
	if (mediaserver == NULL) {
		debugf(_DBG, "upnpd_mediaserver_init() failed");
		goto out;
	}

	rline = NULL;
	rcommand = NULL;
	running = 1;
	while (running) {
#if HAVE_LIBREADLINE
		if (mediaserver->daemonize == 0) {
			rline = readline("console> ");
			rcommand = mediaserver_rline_strip(rline);
			if (rcommand && *rcommand) {
				add_history(rcommand);
				if (mediaserver_rline_process(mediaserver, rcommand) != 0) {
					running = 0;
				}
			}
			free(rline);
		} else {
			upnpd_time_sleep(2);
		}
#else
		upnpd_time_sleep(2);
#endif
	};

	rc = upnpd_mediaserver_uninit(mediaserver);
	if (rc != 0) {
		debugf(_DBG, "upnpd_mediaserver_uninit(mediaserver) failed");
		goto out;
	}
	ret = 0;
out:	return ret;
}

upnpd_application_t mediaserver = {
	.name = "mediaserver",
	.description = "upnp av mediaserver device",
	.main = &mediaserver_main,
};
