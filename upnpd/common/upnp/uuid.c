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
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>

#include "platform.h"
#include "uuid.h"

void uuid_generate (uuid_t *uuid)
{
	unsigned long long tm;
	const char *uuid_format = "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x";
	tm = time_gettimeofday();
	memset(uuid, 0, sizeof(uuid_t));
	uuid->time_low = (tm >> 32) & 0xffffffff;
	uuid->time_mid = (tm >> 16) & 0x0000ffff;
	uuid->time_high_version = tm & 0xffff;
	uuid->clock_seq = 0;
	uuid->node[0] = rand_rand() & 0xff;
	uuid->node[1] = rand_rand() & 0xff;
	uuid->node[2] = rand_rand() & 0xff;
	uuid->node[3] = rand_rand() & 0xff;
	uuid->node[4] = rand_rand() & 0xff;
	uuid->node[5] = rand_rand() & 0xff;
	sprintf(uuid->uuid,
		uuid_format,
		uuid->time_low,
		uuid->time_mid,
		uuid->time_high_version,
		uuid->clock_seq,
		uuid->node[0], uuid->node[1], uuid->node[2],
		uuid->node[3], uuid->node[4], uuid->node[5]);
}
