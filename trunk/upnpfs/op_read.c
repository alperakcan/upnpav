/**
 * Copyright (c) 2009 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the fuse-ext2
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <assert.h>

#include "upnpfs.h"

typedef struct upnpfs_http_s {
	upnp_url_t url;
	char *buffer;
	unsigned int buffer_length;
	char *lines;
	unsigned int lines_length;
	unsigned int lines_count;
	unsigned int offset;
	unsigned int size;
	int file;
	int code;
	int seek;
} upnpfs_http_t;

static int http_read (upnpfs_http_t *http, char *buffer, unsigned int size)
{
	int r;
	int t;
	int rc;
	struct pollfd pfd;
	t = 0;
	while (t != size) {
		memset(&pfd, 0, sizeof(struct pollfd));
		pfd.fd = http->file;
		pfd.events = POLLIN;
		rc = poll(&pfd, 1, 1000);
		if (rc <= 0 || pfd.revents != POLLIN) {
			debugfs("poll() %d failed", rc);
			return 0;
		}
		r = recv(pfd.fd, buffer + t, size - t, 0);
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
	struct pollfd pfd;
	t = 0;
	while (t != size) {
		memset(&pfd, 0, sizeof(struct pollfd));
		pfd.fd = http->file;
		pfd.events = POLLOUT;
		rc = poll(&pfd, 1, 1000);
		if (rc <= 0 || pfd.revents != POLLOUT) {
			debugfs("poll() failed");
			return 0;
		}
		s = send(pfd.fd, buffer + t, size - t, 0);
		if (s <= 0) {
			debugfs("send() failed");
			break;
		}
		t += s;
	}
	return t;
}

static int http_getline (int fd, int timeout, char *buf, int buflen)
{
	int rc;
	struct pollfd pfd;
	int count = 0;
	char c;

	memset(&pfd, 0, sizeof(struct pollfd));
	pfd.fd = fd;
	pfd.events = POLLIN;

	rc = poll(&pfd, 1, timeout);
	if (rc <= 0 || pfd.revents != POLLIN) {
		return 0;
	}

	while (1) {
		if (recv(pfd.fd, &c, 1, MSG_DONTWAIT) <= 0) {
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
	if (http->file >= 0) {
		close(http->file);
	}
	http->file = -1;
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
	struct sockaddr_in server;

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(http->url.host);
	server.sin_port = htons(http->url.port);

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

	http->file = socket(AF_INET, SOCK_STREAM, 0);
	if (http->file < 0) {
		debugfs("socket() failed");
		goto error;
	}
	if (connect(http->file, (struct sockaddr *) &server, sizeof(server)) == -1) {
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

	while (1) {
		if (http_getline(http->file, 1000, http->lines, http->lines_length) <= 0) {
			break;
		}
        	err = http_process(http, http->lines, 1);
        	if (err < 0) {
        		debugfs("http_process() failed");
        		return err;
        	}
        	if (err == 0) {
        		break;
        	}
	}

	return (offset == http->offset) ? 0 : -1;
error:
	http_close(http);
	debugfs("failed");
	return -1;
}

int op_read (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int len;
	upnpfs_http_t h;
	upnpfs_cache_t *c;
	debugfs("enter");
	c = (upnpfs_cache_t *) (unsigned long) fi->fh;
	memset(&h, 0, sizeof(upnpfs_http_t));
	if (upnp_url_parse(c->source, &h.url) != 0) {
		debugf("upnp_url_parse('%s') failed", c->source);
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
