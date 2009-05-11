/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 - 2009 by Alper Akcan
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
ssdp_t * ssdp_init (int (*callback) (void *cookie, ssdp_event_t *event), void *cookie);
int ssdp_uninit (ssdp_t *ssdp);
