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

#include "upnpfs.h"

#include <unistd.h>
#include <ctype.h>
#include <assert.h>

typedef struct upnpfs_http_s {
	upnp_url_t url;
	char *buffer;
	unsigned int buffer_length;
	unsigned long long offset;
	unsigned long long size;
	socket_t *socket;
	int code;
	int seek;
} upnpfs_http_t;

static int http_read (upnpfs_http_t *http, char *buffer, unsigned int size)
{
	int r;
	int t;
	int rc;
	socket_event_t presult;
	t = 0;
	while (t != size) {
		rc = socket_poll(http->socket, SOCKET_EVENT_IN, &presult, 1000);
		if (rc <= 0 || presult != SOCKET_EVENT_IN) {
			debugfs("poll() %d failed", rc);
			return 0;
		}
		r = socket_recv(http->socket, buffer + t, size - t);
		if (r <= 0) {
			debugfs("recv() failed");
			break;
		}
		t += r;
	}
	return t;
}

static int http_write (upnpfs_http_t *http, char *buffer, unsigned int size)
{
	int s;
	int t;
	int rc;
	socket_event_t presult;
	t = 0;
	while (t != size) {
		rc = socket_poll(http->socket, SOCKET_EVENT_OUT, &presult, 1000);
		if (rc <= 0 || presult != SOCKET_EVENT_OUT) {
			debugfs("poll() failed");
			return 0;
		}
		s = socket_send(http->socket, buffer + t, size - t);
		if (s <= 0) {
			debugfs("send() failed");
			break;
		}
		t += s;
	}
	return t;
}

static int http_getline (socket_t *socket, int timeout, char *buf, int buflen)
{
	char c;
	int rc;
	int count;
	socket_event_t presult;

	count = 0;
	rc = socket_poll(socket, SOCKET_EVENT_IN, &presult, timeout);
	if (rc <= 0 || presult != SOCKET_EVENT_IN) {
		return 0;
	}

	while (1) {
		if (socket_recv(socket, &c, 1) <= 0) {
			break;
		}
		buf[count] = c;
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			buf[count] = '\0';
			return count;
		}
		if (count < buflen - 1) {
			count++;
		}
	}
	return count;
}

static int http_close (upnpfs_http_t *http)
{
	if (http == NULL) {
		return 0;
	}
	free(http->buffer);
	if (http->socket != NULL) {
		socket_close(http->socket);
	}
	free(http);
	return 0;
}

static int http_process (upnpfs_http_t *http, char *line, int line_count)
{
	char *p;
	char *tag;
	const char *slash;

	/* end of header */
	if (line[0] == '\0') {
		return 0;
	}

	p = line;
	if (line_count == 0) {
		while (!isspace(*p) && *p != '\0') {
			p++;
		}
		while (isspace(*p)) {
			p++;
		}
		http->code = strtol(p, NULL, 10);
		/* error codes are 4xx and 5xx */
		if (http->code >= 400 && http->code < 600) {
			debugfs("http->code: %d\n", http->code);
			return -1;
		}
	} else {
		while (*p != '\0' && *p != ':') {
			p++;
		}
		if (*p != ':') {
			return 1;
		}
		*p = '\0';
		tag = line;
		p++;
		while (isspace(*p)) {
			p++;
		}
		if (!strcmp(tag, "Location")) {
			/* new location */
		} else if (!strcmp (tag, "Content-Length") && http->size == 0) {
			http->size = atoll(p);
		} else if (!strcmp (tag, "Content-Range")) {
			/* "bytes $from-$to/$document_size" */
			if (!strncmp (p, "bytes ", 6)) {
				p += 6;
				http->offset = atoll(p);
				if ((slash = strchr(p, '/')) && strlen(slash) > 0) {
					http->size = atoll(slash + 1);
				}
			}
			http->seek = 1;
		}
	}

	return 1;
}

static upnpfs_http_t * http_open (const char *path, unsigned long long offset)
{
	int err;
	int line;
	upnpfs_http_t *h;

	h = (upnpfs_http_t *) malloc(sizeof(upnpfs_http_t));
	if (h == NULL) {
		return NULL;
	}
	memset(h, 0, sizeof(upnpfs_http_t));
	if (upnp_url_parse(path, &h->url) != 0) {
		debugf("upnp_url_parse('%s') failed", path);
		goto error;
	}
	h->socket = socket_open(SOCKET_TYPE_STREAM);
	if (h->socket == NULL) {
		debugfs("socket() failed");
		goto error;
	}
	if (socket_connect(h->socket, h->url.host, h->url.port, 10000) < 0) {
		debugfs("connect() failed");
		goto error;
	}

	h->buffer_length = 4096;
	h->buffer = (char *) malloc(h->buffer_length);
	if (h->buffer == NULL) {
		debugfs("malloc() failed");
		goto error;
	}

	snprintf(h->buffer, h->buffer_length,
			"GET /%s HTTP/1.1\r\n"
			"User-Agent: upnpfs\r\n"
			"Accept: */*\r\n"
			"Range: bytes=%llu-\r\n"
			"Host: %s:%u\r\n"
			"Connection: close\r\n"
			"\r\n",
			h->url.path,
			offset,
			h->url.host,
			h->url.port);
	if (http_write(h, h->buffer, strlen(h->buffer)) <= 0) {
		debugfs("http_write() failed");
		goto error;
	}

	line = 0;
	while (1) {
		if (http_getline(h->socket, 10000, h->buffer, h->buffer_length) <= 0) {
			break;
		}
        	err = http_process(h, h->buffer, line);
        	if (err < 0) {
        		debugfs("http_process() failed with '%d'", err);
        		goto error;
        	}
        	if (err == 0) {
        		break;
        	}
        	line++;
	}

	if (offset != h->offset) {
		goto error;
	}
	return h;
error:
	http_close(h);
	debugfs("http_opem failed for '%s'", path);
	return NULL;
}

int do_releasefile (upnpfs_file_t *file)
{
	if (file) {
		do_releasecache(file->cache);
		http_close(file->protocol);
		file->cache = NULL;
		file->protocol = NULL;
	}
	return 0;
}
int op_read (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int siz;
	int len;
	entry_t *e;
	upnpfs_http_t *h;
	upnpfs_http_t *n;
	upnpfs_file_t *f;
	debugfs("enter (fi: %p)", fi);
	f = (upnpfs_file_t *) (unsigned long) fi->fh;
	if (f->metadata == 1) {
		e = controller_browse_metadata(priv.controller, f->cache->device, f->cache->object);
		if (e == NULL) {
			debugfs("controller_browse_metadata() failed");
			return -EIO;
		}
		if (e->metadata == NULL) {
			debugfs("no metadata information");
			entry_uninit(e);
			return -EIO;
		}
		len = strlen(e->metadata);
		if (offset >= len) {
			entry_uninit(e);
			return 0;
		}
		siz = (size < (len - offset)) ? size : (len - offset);
		memcpy(buf, e->metadata, siz);
		entry_uninit(e);
		debugfs("leave, size: %u, offset: %u, len: %d", (unsigned int) size, (unsigned int) offset, siz);
		return siz;
	} else {
		if (f->protocol == NULL) {
			h = http_open(f->cache->source, offset);
			if (h == NULL) {
				debugf("http_open('%s', '%llu') failed", f->cache->source, offset);
				return -EIO;
			}
			f->protocol = (void *) h;
		}
		h = (upnpfs_http_t *) f->protocol;
		if (h->offset != offset) {
			n = http_open(f->cache->source, offset);
			if (n == NULL) {
				debugf("http_open('%s', '%llu') failed", f->cache->source, offset);
			} else {
				f->protocol = n;
				http_close(h);
			}
			f->protocol = (void *) n;
			h = (upnpfs_http_t *) f->protocol;
		}
		len = http_read(h, buf, size);
		if (len > 0) {
			h->offset += len;
		}
		//http_close(h);
		//f->protocol = NULL;
		debugfs("leave, size: %u, offset: %u, len: %d", (unsigned int) size, (unsigned int) offset, len);
		return len;
	}
}
