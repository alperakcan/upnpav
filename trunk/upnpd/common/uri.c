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
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>

#include "platform.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

#define HEXCHRLC(x) ((x) >= 10 ? 'a' + (x) : '0' + (x))
#define HEXCHRUC(x) ((x) >= 10 ? 'A' + (x) : '0' + (x))

static void uri_escape_real (const char *str, char *target, uint32_t *length)
{
	if (target != NULL) {
		uint32_t len = 0;
		for (; *str; str++) {
			if (!isalnum(*str) && *str != '/' && *str != '.') {
				target[len++] = '%';
				target[len++] = HEXCHRUC(*str >> 4);
				target[len++] = HEXCHRUC(*str & 0x0F);
			} else {
				target[len++] = *str;
			}
		}
		target[len] = '\0';

		if (length != NULL)
			*length = len;
	} else if (length != NULL) {
		uint32_t len = 0;

		for (; *str; str++) {
			if (!isalnum(*str) && *str != '/' && *str != '.') {
				len += 3;
			} else {
				len++;
			}
		}

		*length = len;
	}
}

char * uri_escape (const char *str)
{
	uint32_t len;
	char *out;

	uri_escape_real(str, NULL, &len);
	out = malloc(len + 1);
	if (out == NULL) {
		debugf("malloc(len + 1) failed");
		return NULL;
	}
	uri_escape_real(str, out, NULL);
	return out;
}
