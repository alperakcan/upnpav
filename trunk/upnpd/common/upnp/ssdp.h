/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2010 Alper Akcan, alper.akcan@gmail.com
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

typedef struct ssdp_s ssdp_t;
typedef struct ssdp_device_s ssdp_device_t;

typedef enum {
	SSDP_EVENT_TYPE_UNKNOWN,
	SSDP_EVENT_TYPE_NOTIFY,
	SSDP_EVENT_TYPE_BYEBYE,
} ssdp_event_type_t;

typedef struct ssdp_event_s {
	ssdp_event_type_t type;
	char *device;
	char *location;
	char *uuid;
	int expires;
} ssdp_event_t;

int ssdp_search (ssdp_t *ssdp, const char *device, const int timeout);
int ssdp_advertise (ssdp_t *ssdp);
int ssdp_register (ssdp_t *ssdp, char *nt, char *usn, char *location, char *server, int age);
ssdp_t * ssdp_init (const char *address, const char *netmask, int (*callback) (void *cookie, ssdp_event_t *event), void *cookie);
int ssdp_uninit (ssdp_t *ssdp);
