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
#include <inttypes.h>

#include "platform.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

#include "controller.h"

struct upnpd_controller_s {
	char *interface;
	char *ipaddr;
	char *netmask;

	client_t *client;
};

upnpd_controller_t * upnpd_controller_init (const char *interface)
{
	char *opt;
	upnpd_controller_t *c;

	c = (upnpd_controller_t *) malloc(sizeof(upnpd_controller_t));
	if (c == NULL) {
		goto err0;
	}
	memset(c, 0, sizeof(upnpd_controller_t));

	c->interface = strdup(interface);
	c->ipaddr = interface_getaddr(interface);
	c->netmask = interface_getmask(interface);

	if (c->interface == NULL ||
	    c->ipaddr == NULL ||
	    c->netmask == NULL) {
		goto err1;
	}

	if (asprintf(&opt, "ipaddr=%s,netmask=%s", c->ipaddr, c->netmask) < 0) {
		goto err1;
	}

	c->client = controller_init(opt);
	if (c->client == NULL) {
		goto err2;
	}

	free(opt);
	return c;

err2:	free(opt);
err1:	free(c->netmask);
	free(c->ipaddr);
	free(c->interface);
	free(c);
err0:	return NULL;
}

int upnpd_controller_uninit (upnpd_controller_t *controller)
{
	free(controller->interface);
	free(controller->ipaddr);
	free(controller->netmask);
	controller_uninit(controller->client);
	free(controller);
	return 0;
}

int upnpd_controller_scan_devices (upnpd_controller_t *controller, int remove)
{
	return client_refresh(controller->client, remove);
}

upnpd_device_t * upnpd_controller_get_devices (upnpd_controller_t *controller)
{
	client_t *client;
	client_device_t *device;

	upnpd_device_t *d;
	upnpd_device_t *r;
	upnpd_device_t *t;

	r = NULL;
	client = controller->client;

	thread_mutex_lock(client->mutex);
	list_for_each_entry(device, &client->devices, head) {
		d = (upnpd_device_t *) malloc(sizeof(upnpd_device_t));
		if (d == NULL) {
			continue;
		}
		memset(d, 0, sizeof(upnpd_device_t));
		d->name = strdup(device->name);
		d->type = strdup(device->type);
		d->uuid = strdup(device->uuid);
		d->location = strdup(device->location);
		d->expiretime = device->expiretime;
		if (d->name == NULL ||
		    d->type == NULL ||
		    d->uuid == NULL ||
		    d->location == NULL) {
			free(d->name);
			free(d->type);
			free(d->uuid);
			free(d->location);
			free(d);
			continue;
		}
		if (r == NULL) {
			r = d;
		} else {
			t->next = d;
		}
		t = d;
	}
	thread_mutex_unlock(client->mutex);

	return r;
}

int upnpd_controller_free_devices (upnpd_device_t *device)
{
	upnpd_device_t *t;
	while (device) {
		t = device;
		device = device->next;
		free(t->name);
		free(t->type);
		free(t->uuid);
		free(t->location);
		free(t);
	}
	return 0;
}

upnpd_item_t * upnpd_controller_browse_device (upnpd_controller_t *controller, const char *device, const char *item)
{
	entry_t *e;
	entry_t *entry;
	client_t *client;

	upnpd_item_t *i;
	upnpd_item_t *r;
	upnpd_item_t *t;

	r = NULL;
	client = controller->client;

	entry = controller_browse_children(client, device, (item == NULL) ? "0" : item);

	for (e = entry; e; e = e->next) {
		i = (upnpd_item_t *) malloc(sizeof(upnpd_item_t));
		if (i == NULL) {
			continue;
		}
		memset(i, 0, sizeof(upnpd_item_t));
		i->id = e->didl.entryid;
		i->title = e->didl.dc.title;
		i->class = e->didl.upnp.object.class;
		i->location = e->didl.res.path;
		if (i->id == NULL ||
		    i->title == NULL ||
		    i->class == NULL) {
			free(i);
			continue;
		}
		e->didl.entryid = NULL;
		e->didl.dc.title = NULL;
		e->didl.upnp.object.class = NULL;
		e->didl.res.path = NULL;
		if (r == NULL) {
			r = i;
		} else {
			t->next = i;
		}
		t = i;
	}

	entry_uninit(entry);

	return r;
}

upnpd_item_t * upnpd_controller_metadata_device (upnpd_controller_t *controller, const char *device, const char *item)
{
	entry_t *entry;
	client_t *client;

	upnpd_item_t *i;

	i = NULL;
	client = controller->client;

	entry = controller_browse_metadata(client, device, (item == NULL) ? "0" : item);
	if (entry != NULL) {
		i = (upnpd_item_t *) malloc(sizeof(upnpd_item_t));
		if (i == NULL) {
			goto out;
		}
		memset(i, 0, sizeof(upnpd_item_t));
		i->id = entry->didl.entryid;
		i->title = entry->didl.dc.title;
		i->class = entry->didl.upnp.object.class;
		i->location = entry->didl.res.path;
		if (i->id == NULL ||
		    i->title == NULL ||
		    i->class == NULL) {
			free(i);
			i = NULL;
			goto out;
		}
		entry->didl.entryid = NULL;
		entry->didl.dc.title = NULL;
		entry->didl.upnp.object.class = NULL;
		entry->didl.res.path = NULL;
	}

out:	entry_uninit(entry);
	return i;
}

int upnpd_controller_free_items (upnpd_item_t *item)
{
	upnpd_item_t *t;
	while (item) {
		t = item;
		item = item->next;
		free(t->id);
		free(t->title);
		free(t->class);
		free(t->location);
		free(t);
	}
	return 0;
}
