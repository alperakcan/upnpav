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
	char *lines;
	unsigned int lines_length;
	unsigned int lines_count;
	unsigned int offset;
	unsigned int size;
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
	free(http->lines);
	http->lines = NULL;
	free(http->buffer);
	http->buffer = NULL;
	if (http->socket != NULL) {
		socket_close(http->socket);
	}
	http->socket = NULL;
	http->buffer_length =  0;
	http->lines_length = 0;
	http->size = 0;
	http->offset = 0;
	http->size = 0;
	http->code = 0;
	http->seek = 0;
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
				http->offset = atol(p);
				if ((slash = strchr(p, '/')) && strlen(slash) > 0) {
					http->size = atoll(slash + 1);
				}
			}
			http->seek = 1;
		}
	}

	return 1;
}

static int http_open (upnpfs_http_t *http, unsigned int offset)
{
	int err;
	int line;

	http->lines = NULL;
	http->buffer = NULL;
	http->buffer_length =  0;
	http->lines_length = 0;
	http->lines = 0;
	http->size = 0;
	http->offset = 0;
	http->size = 0;
	http->code = 0;
	http->seek = 0;

	http->socket = socket_open(SOCKET_TYPE_STREAM);
	if (http->socket == NULL) {
		debugfs("socket() failed");
		goto error;
	}
	if (socket_connect(http->socket, http->url.host, http->url.port, 10000) < 0) {
		debugfs("connect() failed");
		goto error;
	}

	http->lines = (char *) malloc(sizeof(unsigned char) * 4096);
	http->buffer = (char *) malloc(sizeof(unsigned char) * 4096);
	if (http->buffer == NULL || http->lines == NULL) {
		debugfs("malloc() failed");
		goto error;
	}
	http->buffer_length = 4096;
	http->lines_length = 4096;

	snprintf(http->buffer, 4096,
			"GET /%s HTTP/1.1\r\n"
			"User-Agent: upnpfs\r\n"
			"Accept: */*\r\n"
			"Range: bytes=%u-\r\n"
			"Host: %s:%u\r\n"
			"Connection: close\r\n"
			"\r\n",
			http->url.path,
			offset,
			http->url.host,
			http->url.port);
	if (http_write(http, http->buffer, strlen(http->buffer)) <= 0) {
		debugfs("http_write() failed");
		goto error;
	}

	line = 0;
	while (1) {
		if (http_getline(http->socket, 1000, http->lines, http->lines_length) <= 0) {
			break;
		}
        	err = http_process(http, http->lines, line);
        	if (err < 0) {
        		debugfs("http_process() failed");
        		return err;
        	}
        	if (err == 0) {
        		break;
        	}
        	line++;
	}

	return (offset == http->offset) ? 0 : -1;
error:
	http_close(http);
	debugfs("failed");
	return -1;
}

int op_read (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int siz;
	int len;
	entry_t *e;
	upnpfs_http_t h;
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
		memset(&h, 0, sizeof(upnpfs_http_t));
		if (upnp_url_parse(f->cache->source, &h.url) != 0) {
			debugf("upnp_url_parse('%s') failed", f->cache->source);
			return -EIO;
		}
		if (http_open(&h, offset) != 0) {
			debugfs("http_open() failed");
			return -EIO;
		}
		if (h.buffer_length < size) {
			h.buffer = (char *) realloc(h.buffer, sizeof(unsigned char) * size);
			if (h.buffer == NULL) {
				http_close(&h);
				return -EIO;
			}
			h.buffer_length = size;
		}
		len = http_read(&h, h.buffer, size);
		memcpy(buf, h.buffer, len);
		if (len > 0) {
			h.offset += len;
		}
		http_close(&h);
		upnp_url_uninit(&h.url);
		debugfs("leave, size: %u, offset: %u, len: %d", (unsigned int) size, (unsigned int) offset, len);
		return len;
	}
}
