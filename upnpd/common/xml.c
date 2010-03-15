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
#include <inttypes.h>

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

static void xmlescape_real (const char *str, char *target, int * length, int attribute)
{
	if (target != NULL) {
		int len = 0;

		for (; *str; str++) {
			if (*str == '<') {
				memcpy(target + len, "&lt;", 4);
				len += 4;
			} else if (attribute && (*str == '"')) {
				memcpy(target + len, "%22", 3);
				len += 3;
			} else if (*str == '>') {
				memcpy(target + len, "&gt;", 4);
				len += 4;
			} else if (*str == '&') {
				memcpy(target + len, "&amp;", 5);
				len += 5;
			} else {
				target[len++] = *str;
			}
		}
		target[len] = '\0';

		if (length != NULL)
			*length = len;
	} else if (length != NULL) {
		int len = 0;

		for (; *str; str++) {
			if (*str == '<') {
				len += 4;
			} else if (attribute && (*str == '"')) {
				len += 3;
			} else if (*str == '>') {
				len += 4;
			} else if (*str == '&') {
				len += 5;
			} else {
				len++;
			}
		}

		*length = len;
	}
}

char * upnpd_xml_escape (const char *str, int attribute)
{
	int len;
	char *out;

	if (str == NULL) {
		return NULL;
	}
	xmlescape_real(str, NULL, &len, attribute);
	out = malloc(len + 1);
	xmlescape_real(str, out, NULL, attribute);
	return out;
}
