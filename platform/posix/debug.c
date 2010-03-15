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
#include <stdarg.h>
#include <inttypes.h>

#include "platform.h"

int platform_debug = 0;

void upnpd_debug_debugf (char *file, int line, const char *func, char *fmt, ...)
{
	int n;
	int s;
	char *p;
	va_list args;

	if (platform_debug == 0) {
		return;
	}

	s = 100;
	if ((p = malloc(sizeof(char) * s)) == NULL) {
		fprintf(stderr, "exiting [%s (%s:%d)]\n", __FUNCTION__, __FILE__, __LINE__);
		goto err;
	}

	while (1) {
		va_start(args, fmt);
		n = vsnprintf(p, s, fmt, args);
		va_end(args);
		if (n > -1 && n < s) {
			break;
		}
		if (n > -1) {
			s = n + 1;
		} else {
			s *= 2;
		}
		if ((p = realloc(p, s))  == NULL) {
			goto err;
		}
	}

	fprintf(stderr, "\033[1;30m[0x%08X] \033[0m%s \033[1;30m[%s (%s:%d)]\033[0m\n", (unsigned int) upnpd_thread_self(), p, func, file, line);
	fflush(stderr);
	free(p);

	return;
err:	exit(1);
}

