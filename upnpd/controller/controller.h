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
	/** device name */
	char *name;
	/** device type */
	char *type;
	/** device uuid */
	char *uuid;
	/** device description url */
	char *location;
	/** device expire time */
	int expiretime;
	/** next device */
	upnpd_device_t *next;
};

struct upnpd_item_s {
	/** item id */
	char *id;
	/** item parent id */
	char *pid;
	/** item title */
	char *title;
	/** item class */
	char *class;
	/** item fetch url */
	char *location;
	/** item size in bytes */
	unsigned long long size;
	/** item duration */
	char *duration;
	/** next item */
	upnpd_item_t *next;
};

upnpd_controller_t * upnpavd_controller_init (const char *interface);

int upnpavd_controller_uninit (upnpd_controller_t *controller);

int upnpavd_controller_scan_devices (upnpd_controller_t *controller, int remove);

upnpd_device_t * upnpavd_controller_get_devices (upnpd_controller_t *controller);

int upnpavd_controller_free_devices (upnpd_device_t *device);

upnpd_item_t * upnpavd_controller_browse_device (upnpd_controller_t *controller, const char *device, const char *item);

upnpd_item_t * upnpavd_controller_metadata_device (upnpd_controller_t *controller, const char *device, const char *item);

upnpd_item_t * upnpavd_controller_browse_local (const char *path);

int upnpavd_controller_free_items (upnpd_item_t *item);
