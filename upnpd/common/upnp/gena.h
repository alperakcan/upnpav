/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 - 20010 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 - 20010 CoreCodec, Inc., http://www.CoreCodec.com
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

typedef struct gena_s gena_t;

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
	GENA_EVENT_TYPE_SUBSCRIBE_DROP    = 0x04,
	GENA_EVENT_TYPE_ACTION            = 0x05,
} gena_event_type_t;

typedef struct gena_file_s {
	int virtual;
	int fd;
	unsigned int size;
	unsigned int offset;
	char *buf;
	void *data;
} gena_file_t;

typedef struct gena_fileinfo_s {
	unsigned long long size;
	char *mimetype;
	unsigned long mtime;
	int seekable;
} gena_fileinfo_t;

typedef struct gena_callback_vfs_s {
	int (*info) (void *cookie, char *path, gena_fileinfo_t *info);
	void * (*open) (void *cookie, char *path, gena_filemode_t mode);
	int (*read) (void *cookie, void *handle, char *buffer, unsigned int length);
	int (*write) (void *cookie, void *handle, char *buffer, unsigned int length);
	unsigned long long (*seek)  (void *cookie, void *handle, unsigned long long offset, gena_seek_t whence);
	int (*close) (void *cookie, void *handle);
	void *cookie;
} gena_callback_vfs_t;

typedef struct gena_event_unsubscribe_s {
	char *path;
	char *host;
	char *sid;
} gena_event_unsubscribe_t;

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
		gena_event_unsubscribe_t unsubscribe;
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

char * upnp_gena_download (gena_t *gena, const char *host, const unsigned short port, const char *path);
char * upnp_gena_send_recv (gena_t *gena, const char *host, const unsigned short port, const char *header, const char *data);
unsigned short upnp_gena_getport (gena_t *gena);
const char * upnp_gena_getaddress (gena_t *gena);
gena_t * upnp_gena_init (char *address, unsigned short port, gena_callbacks_t *callbacks);
int upnp_gena_uninit (gena_t *gena);
