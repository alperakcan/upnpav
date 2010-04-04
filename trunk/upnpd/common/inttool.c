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

#include <stdio.h>
#include <string.h>

#include "platform.h"

char * upnpd_uint32tostr (char *out, uint32_t val)
{
	sprintf(out, "%u", val);
	return out;
}

uint32_t upnpd_strtouint32 (const char *in)
{
	uint32_t r;
	if (in != NULL) {
		if (strcasecmp(in, "true") == 0) {
			return 1;
		} else if (strcasecmp(in, "false") == 0) {
			return 0;
		} else if (sscanf(in, "%u", &r) == 1) {
			return r;
		}
	}
	return 0;
}

int32_t upnpd_strtoint32 (const char *in)
{
	int32_t r;
	if (in != NULL) {
		if (strcasecmp(in, "true") == 0) {
			return 1;
		} else if (strcasecmp(in, "false") == 0) {
			return 0;
		} else if (sscanf(in, "%d", &r) == 1) {
			return r;
		}
	}
	return 0;
}
