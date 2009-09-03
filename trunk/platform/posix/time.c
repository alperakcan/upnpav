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

#include <unistd.h>
#include <time.h>
#include <sys/time.h>

int time_sleep (unsigned int secs)
{
	return sleep(secs);
}

int time_usleep (unsigned int usecs)
{
	return usleep(usecs);
}

unsigned long long time_gettimeofday (void)
{
	long long tsec;
	long long tusec;
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		return 0;
	}
	tsec = ((long long) tv.tv_sec) * 1000;
	tusec = ((long long) tv.tv_usec) / 1000;
	return tsec + tusec;
}
