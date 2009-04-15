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
#include <ctype.h>
#include <inttypes.h>

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
