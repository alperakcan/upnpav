/***************************************************************************
    begin                : Mon Mar 02 2009
    copyright            : (C) 2009 by Alper Akcan
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

#ifndef UPNP_H_
#define UPNP_H_

#include <upnp/ixml.h>

typedef struct ssdp_s ssdp_t;
typedef struct ssdp_device_s ssdp_device_t;
typedef struct gena_s gena_t;
typedef struct upnp_s upnp_t;

typedef enum {
	GENA_FILEMODE_READ  = 0x01,
	GENA_FILEMODE_WRITE = 0x02,
} gena_filemode_t;

typedef enum {
	GENA_SEEK_SET = 0x01,
	GENA_SEEK_CUR = 0x02,
	GENA_SEEK_END = 0x03,
} gena_seek_t;

typedef enum {
	GENA_EVENT_TYPE_UNKNOWN           = 0x00,
	GENA_EVENT_TYPE_SUBSCRIBE_REQUEST = 0x01,
	GENA_EVENT_TYPE_SUBSCRIBE_ACCEPT  = 0x02,
	GENA_EVENT_TYPE_SUBSCRIBE_RENEW   = 0x03,
	GENA_EVENT_TYPE_ACTION            = 0x04,
} gena_event_type_t;

typedef struct gena_file_s {
	int virtual;
	int fd;
	unsigned int size;
	unsigned int offset;
	char *buf;
} gena_file_t;

typedef struct gena_fileinfo_s {
	unsigned long size;
	char *mimetype;
	unsigned long mtime;
} gena_fileinfo_t;

typedef struct gena_callback_vfs_s {
	int (*info) (void *cookie, char *path, gena_fileinfo_t *info);
	void * (*open) (void *cookie, char *path, gena_filemode_t mode);
	int (*read) (void *cookie, void *handle, char *buffer, unsigned int length);
	int (*write) (void *cookie, void *handle, char *buffer, unsigned int length);
	unsigned long (*seek)  (void *cookie, void *handle, long offset, gena_seek_t whence);
	int (*close) (void *cookie, void *handle);
	void *cookie;
} gena_callback_vfs_t;

typedef struct gena_event_subscribe_s {
	char *path;
	char *host;
	char *nt;
	char *callback;
	char *scope;
	char *timeout;
	char *sid;
} gena_event_subscribe_t;

typedef struct gena_event_action_s {
	char *path;
	char *host;
	char *action;
	char *serviceid;
	char *request;
	char *response;
	unsigned int length;
} gena_event_action_t;

typedef struct gena_event_s {
	gena_event_type_t type;
	union {
		gena_event_subscribe_t subscribe;
		gena_event_action_t action;
	} event;
} gena_event_t;

typedef struct gena_callback_gena_s {
	int (*event) (void *cookie, gena_event_t *);
	void *cookie;
} gena_callback_gena_t;

typedef struct gena_callbacks_s {
	gena_callback_vfs_t vfs;
	gena_callback_gena_t gena;
} gena_callbacks_t;

int ssdp_advertise (ssdp_t *ssdp);
int ssdp_register (ssdp_t *ssdp, char *nt, char *usn, char *location, char *server, int age);
ssdp_t * ssdp_init (void);
int ssdp_uninit (ssdp_t *ssdp);

int gena_send_recv (gena_t *gena, const char *host, const unsigned short port, const char *header, const char *data);
unsigned short gena_getport (gena_t *gena);
const char * gena_getaddress (gena_t *gena);
gena_t * gena_init (char *address, unsigned short port, gena_callbacks_t *callbacks);
int gena_uninit (gena_t *gena);

typedef enum {
	UPNP_EVENT_TYPE_UNKNOWN           = 0x00,
	UPNP_EVENT_TYPE_SUBSCRIBE_REQUEST = 0x01,
	UPNP_EVENT_TYPE_ACTION            = 0x02,
} upnp_event_type_t;

typedef struct upnp_event_subscribe_s {
	char *udn;
	char *serviceid;
	char *sid;
} upnp_event_subscribe_t;

typedef struct upnp_event_action_s {
	char *udn;
	char *serviceid;
	char *action;
	IXML_Document *request;
	int errcode;
	IXML_Document *response;
} upnp_event_action_t;

typedef struct upnp_event_s {
	upnp_event_type_t type;
	union {
		upnp_event_subscribe_t subscribe;
		upnp_event_action_t action;
	} event;
} upnp_event_t;

int upnp_advertise (upnp_t *upnp);
int upnp_register_device (upnp_t *upnp, const char *description, int (*callback) (void *cookie, upnp_event_t *), void *cookie);
char * upnp_getaddress (upnp_t *upnp);
unsigned short upnp_getport (upnp_t *upnp);
upnp_t * upnp_init (const char *host, const unsigned short port);
int upnp_uninit (upnp_t *upnp);
int upnp_accept_subscription (upnp_t *upnp, const char *udn, const char *serviceid, const char **variable_names, const char **variable_values, const unsigned int variables_count, const char *sid);
int upnp_addtoactionresponse (upnp_event_action_t *response, const char *service, const char *variable, const char *value);

#endif /* UPNP_H_ */
