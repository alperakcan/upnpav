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

typedef struct upnpd_controller_s upnpd_controller_t;
typedef struct upnpd_device_s upnpd_device_t;
typedef struct upnpd_item_s upnpd_item_t;

struct upnpd_device_s {
	char *name;
	char *type;
	char *uuid;
	char *location;
	int expiretime;
	upnpd_device_t *next;
};

struct upnpd_item_s {
	char *id;
	char *title;
	char *class;
	char *location;
	int64_t size;
	char *duration;
	upnpd_item_t *next;
};

upnpd_controller_t * upnpd_controller_init (const char *interface);

int upnpd_controller_uninit (upnpd_controller_t *controller);

int upnpd_controller_scan_devices (upnpd_controller_t *controller, int remove);

upnpd_device_t * upnpd_controller_get_devices (upnpd_controller_t *controller);

int upnpd_controller_free_devices (upnpd_device_t *device);

upnpd_item_t * upnpd_controller_browse_device (upnpd_controller_t *controller, const char *device, const char *item);

upnpd_item_t * upnpd_controller_metadata_device (upnpd_controller_t *controller, const char *device, const char *item);

int upnpd_controller_free_items (upnpd_item_t *item);
