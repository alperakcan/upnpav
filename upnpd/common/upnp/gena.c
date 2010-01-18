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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "platform.h"
#include "gena.h"
#include "upnp.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))
#define GENA_LISTEN_PORT	10000
#define GENA_LISTEN_MAX	100
#define GENA_HEADER_SIZE	(1024 * 4)
#define GENA_DATA_SIZE	(1024 * 1024)
#define GENA_SOCKET_TIMEOUT	1000

typedef struct gena_filerange_s {
	unsigned long long start;
	unsigned long long stop;
	unsigned long long size;
} gena_filerange_t;

typedef struct gena_fileino_internal_s {
	gena_fileinfo_t fileinfo;
	gena_filerange_t filerange;
} gena_fileinfo_internal_t;

typedef struct gena_thread_s {
	list_t head;
	int running;
	int stopped;
	socket_t *socket;
	thread_t *thread;
	thread_cond_t *cond;
	thread_mutex_t *mutex;
	gena_callbacks_t *callbacks;
} gena_thread_t;

typedef enum {
	GENA_RESPONSE_TYPE_OK,
	GENA_RESPONSE_TYPE_PARTIAL_CONTENT,
	GENA_RESPONSE_TYPE_BAD_REQUEST,
	GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR,
	GENA_RESPONSE_TYPE_NOT_FOUND,
	GENA_RESPONSE_TYPE_PRECONDITION_FAILED,
	GENA_RESPONSE_TYPE_NOT_IMPLEMENTED,
	GENA_RESPONSE_TYPES,
} gena_response_type_t;

typedef struct gena_response_s {
	gena_response_type_t type;
	int code;
	char *name;
	char *info;
} gena_response_t;

struct gena_s {
	int running;
	int stopped;
	socket_t *socket;
	char *address;
	unsigned short port;
	gena_callbacks_t *callbacks;
	thread_t *thread;
	thread_cond_t *cond;
	thread_mutex_t *mutex;
	list_t threads;
};

static const gena_response_t gena_responses[] = {
	{GENA_RESPONSE_TYPE_OK, 200, "OK", NULL},
	{GENA_RESPONSE_TYPE_PARTIAL_CONTENT, 206, "Partial Content", NULL},
	{GENA_RESPONSE_TYPE_BAD_REQUEST, 400, "Bad Request", "Unsupported method"},
	{GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR, 500, "Internal Server Error", "Internal Server Error"},
	{GENA_RESPONSE_TYPE_NOT_FOUND, 404, "Not Found", "The requested URL was not found"},
	{GENA_RESPONSE_TYPE_PRECONDITION_FAILED, 412, "Precondition Failed", "Precondition Failed"},
	{GENA_RESPONSE_TYPE_NOT_IMPLEMENTED, 501, "Not implemented", "Not implemented"},
};

static char * gena_trim (char *buffer)
{
	int l;
	char *out;
	for (out = buffer; *out && (*out == ' ' || *out == '\t'); out++) {
	}
	for (l = strlen(out) - 1; l >= 0; l--) {
		if (out[l] == ' ' || out[l] == '\t') {
			out[l] = '\0';
		} else {
			break;
		}
	}
	for (; *out && (*out == '"' || *out == '\''); out++) {
	}
	for (l = strlen(out) - 1; l >= 0; l--) {
		if (out[l] == '"' || out[l] == '\'') {
			out[l] = '\0';
		} else {
			break;
		}
	}
	return out;
}

static int gena_connect (socket_t *socket, int timeout, const char *host, const unsigned short port)
{
	int rc;
	rc = socket_connect(socket, host, port, timeout);
	if (rc < 0) {
		debugf("connect failed for '%s:%d'", host, port);
		return -1;
	} else {
		debugf("connected to '%s:%d'", host, port);
		return 0;
	}
}

static int gena_send (socket_t *socket, int timeout, const void *buf, unsigned int len)
{
	int t;
	int s;
	int rc;
	socket_event_t presult;
	t = 0;
	while (1) {
		rc = socket_poll(socket, SOCKET_EVENT_OUT, &presult, timeout);
		if (rc == 0) {
			debugf("poll timeout");
			continue;
		}
		if (rc <= 0 || (presult & SOCKET_EVENT_OUT) == 0) {
			debugf("poll failed (%d, 0x%x)", rc, presult);
			break;
		}
		s = socket_send(socket, buf + t, len - t);
		if (s <= 0) {
			break;
		}
		t += s;
	}
	return t;
}

static int gena_getcontent (socket_t *socket, int timeout, char *buf, int buflen)
{
	char c;
	int rc;
	int count;
	socket_event_t presult;
	count = 0;
	while (1) {
		rc = socket_poll(socket, SOCKET_EVENT_IN, &presult, timeout);
		if (rc <= 0 || (presult & SOCKET_EVENT_IN) == 0) {
			debugf("poll failed rc:%d(0x%x)", rc, presult);
			return 0;
		}
		if (socket_recv(socket, &c, 1) <= 0) {
			break;
		}
		buf[count++] = c;
		if (count >= buflen) {
			break;
		}
	}
	return count;
}

static int gena_getline (socket_t *socket, int timeout, char *buf, int buflen)
{
	char c;
	int rc;
	int count;
	socket_event_t presult;
	count = 0;
	while (1) {
		rc = socket_poll(socket, SOCKET_EVENT_IN, &presult, timeout);
		if (rc <= 0 || (presult & SOCKET_EVENT_IN) == 0) {
			debugf("poll failed rc:%d(0x%x)", rc, presult);
			return 0;
		}
		if (socket_recv(socket, &c, 1) <= 0) {
			break;
		}
		buf[count] = c;
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			buf[count] = '\0';
			debugf("%s", buf);
			return count;
		}
		if (count < buflen - 1) {
			count++;
		}
	}
	return count;
}

static int gena_replace_escaped (char *in, int index, size_t *max)
{
	int tempInt = 0;
	char tempChar = 0;
	int i = 0;
	int j = 0;

	if (in[index] && in[index + 1] && in[index + 2]) {
		if (in[index] == '%' && in[index + 1] == '2' && in[index + 2] == 'M') {
			tempChar = ',';
			for (i = index + 3, j = index; j < (*max); i++, j++) {
				in[j] = tempChar;
				if (i < (*max)) {
					tempChar = in[i];
				} else {
					tempChar = 0;
				}
			}
			(*max) -= 2;
			return 1;
		} else if (in[index] == '%' && in[index + 1] == '2' && in[index + 2] == 'N') {
			tempChar = '-';
			for (i = index + 3, j = index; j < (*max); i++, j++) {
				in[j] = tempChar;
				if (i < (*max)) {
					tempChar = in[i];
				} else {
					tempChar = 0;
				}
			}
			(*max) -= 2;
			return 1;
		}
	}
	if ((in[index] == '%') && (isxdigit(in[index + 1])) && isxdigit(in[index + 2])) {
		if (sscanf(&in[index + 1], "%2x", &tempInt) != 1) {
			return 0;
		}
		tempChar = (char) tempInt;
		for (i = index + 3, j = index; j < (*max); i++, j++) {
			in[j] = tempChar;
			if (i < (*max)) {
				tempChar = in[i];
			} else {
				tempChar = 0;
			}
		}
		(*max) -= 2;
		return 1;
	} else {
		return 0;
	}
}

static int gena_remove_escaped_chars (char *in)
{
	int i = 0;
	size_t size;
	size = strlen(in);
	for( i = 0; i < size; i++ ) {
		if (gena_replace_escaped(in, i, &size) != 0) {
			i--;
		}
	}
	return 0;
}

static void gena_senderrorheader (socket_t *socket, gena_response_type_t type)
{
	int len;
	char *header;
	unsigned int i;
	char tmpstr[80];
	int responseNum = 0;
	unsigned long long timer;
	const char *mimetype = NULL;
	const char *infoString = NULL;
	const char *responseString = "";

	timer = time_gettimeofday();
	header = (char *) malloc(GENA_HEADER_SIZE);
	if (header == NULL) {
		return;
	}

	for (i = 0; i < ARRAY_SIZE(gena_responses); i++) {
		if (gena_responses[i].type == type) {
			responseNum = gena_responses[i].code;
			responseString = gena_responses[i].name;
			infoString = gena_responses[i].info;
			break;
		}
	}
	mimetype = "text/html";

	time_strftime(tmpstr, sizeof(tmpstr), timer);
	len = sprintf(header, "HTTP/1.0 %d %s\r\nContent-type: %s\r\n"
			      "Date: %s\r\nConnection: close\r\n",
			      responseNum, responseString, mimetype, tmpstr);

	header[len++] = '\r';
	header[len++] = '\n';
	if (infoString) {
		len += sprintf(header + len,
				"<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n"
				"<BODY><H1>%d %s</H1>\n%s\n</BODY></HTML>\n",
				responseNum, responseString,
				responseNum, responseString, infoString);
	}
	header[len] = '\0';
	debugf("sending header: %s", header);
	if (gena_send(socket, GENA_SOCKET_TIMEOUT, header, len) != len) {
		debugf("send() failed");
	}
	free(header);
}

static void gena_sendfileheader (socket_t *socket, gena_fileinfo_internal_t *fileinfo)
{
	int len;
	char *header;
	unsigned int i;
	char tmpstr[80];
	int responseNum = 0;
	unsigned long long t;
	unsigned long long timer;
	const char *infoString = NULL;
	const char *responseString = "";

	gena_response_type_t type;

	timer = time_gettimeofday();
	header = (char *) malloc(GENA_HEADER_SIZE);
	if (header == NULL) {
		return;
	}

	if (fileinfo->filerange.start == 0 && fileinfo->filerange.stop == 0) {
		type = GENA_RESPONSE_TYPE_OK;
	} else {
		type = GENA_RESPONSE_TYPE_PARTIAL_CONTENT;
	}

	for (i = 0; i < ARRAY_SIZE(gena_responses); i++) {
		if (gena_responses[i].type == type) {
			responseNum = gena_responses[i].code;
			responseString = gena_responses[i].name;
			infoString = gena_responses[i].info;
			break;
		}
	}

	time_strftime(tmpstr, sizeof(tmpstr), timer);
	len = sprintf(header, "HTTP/1.0 %d %s\r\n"
	                      "Content-type: %s\r\n"
			      "Date: %s\r\n"
			      "Server: " SERVER_NAME "\r\n"
			      "Connection: close\r\n",
			      responseNum, responseString, fileinfo->fileinfo.mimetype, tmpstr);

	t = fileinfo->fileinfo.mtime;
	time_strftime(tmpstr, sizeof(tmpstr), t * 1000);
	if (type == GENA_RESPONSE_TYPE_PARTIAL_CONTENT) {
		len += sprintf(header + len,
			"Accept-Ranges: bytes\r\n"
			"Last-Modified: %s\r\n%s %llu\r\n",
			tmpstr,
			"Content-length:",
			fileinfo->filerange.size);
		len += sprintf(header + len,
				"Content-Range: bytes %llu-%llu/%llu\r\n",
				fileinfo->filerange.start,
				fileinfo->filerange.stop,
				fileinfo->fileinfo.size);
	} else {
		len += sprintf(header + len,
			"Accept-Ranges: bytes\r\n"
			"Last-Modified: %s\r\n%s %llu\r\n",
			tmpstr,
			"Content-length:",
			fileinfo->fileinfo.size);
	}

	header[len++] = '\r';
	header[len++] = '\n';
	if (infoString) {
		len += sprintf(header + len,
				"<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n"
				"<BODY><H1>%d %s</H1>\n%s\n</BODY></HTML>\n",
				responseNum, responseString,
				responseNum, responseString, infoString);
	}
	header[len] = '\0';

	debugf("header: %s", header);
	if (gena_send(socket, GENA_SOCKET_TIMEOUT, header, len) != len) {
		debugf("send() failed");
	}
	free(header);
}

static void gena_handler_unsubscribe (gena_thread_t *gena_thread, char *header, const char *path)
{
	gena_event_t event;
	memset(&event, 0, sizeof(gena_event_t));
	event.event.subscribe.path = strdup(path);

	while (1) {
		if (gena_getline(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
			break;
		}
		if (strncasecmp(header, "HOST:", strlen("HOST:")) == 0) {
			event.event.unsubscribe.host = strdup(gena_trim(header + strlen("HOST:")));
		} else if (strncasecmp(header, "SID:", strlen("SID:")) == 0) {
			event.event.unsubscribe.sid = strdup(gena_trim(header + strlen("SID:")));
		}
	}

	debugf("unsubscribe event;\n"
	       "  path    : '%s'\n"
	       "  host    : '%s'\n"
	       "  sid     : '%s'\n",
	       event.event.unsubscribe.path,
	       event.event.unsubscribe.host,
	       event.event.unsubscribe.sid);

	if (event.event.unsubscribe.path == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	if (gena_thread->callbacks == NULL ||
	    gena_thread->callbacks->gena.event == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
		goto out;
	}

	event.type = GENA_EVENT_TYPE_SUBSCRIBE_DROP;
	if (gena_thread->callbacks->gena.event(gena_thread->callbacks->gena.cookie, &event) == 0) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_OK);
		goto out;
	} else {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}

	gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
out:
	free(event.event.unsubscribe.path);
	free(event.event.unsubscribe.host);
	free(event.event.unsubscribe.sid);
}

static void gena_handler_subscribe (gena_thread_t *gena_thread, char *header, const char *path)
{
	const char *fmt =
		"HTTP/1.1 200 OK\r\n"
		"SID: %s\r\n"
		"Timeout: Second-%d\r\n"
		"Connection: close\r\n"
		"Content-Type: text/xml\r\n"
		"Server: " SERVER_NAME "\r\n"
		"Content-Length: 0\r\n"
		"\r\n";

	gena_event_t event;
	memset(&event, 0, sizeof(gena_event_t));
	event.event.subscribe.path = strdup(path);

	while (1) {
		if (gena_getline(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
			break;
		}
		if (strncasecmp(header, "HOST:", strlen("HOST:")) == 0) {
			event.event.subscribe.host = strdup(gena_trim(header + strlen("HOST:")));
		} else if (strncasecmp(header, "NT:", strlen("NT:")) == 0) {
			event.event.subscribe.nt = strdup(gena_trim(header + strlen("NT:")));
		} else if (strncasecmp(header, "CALLBACK:", strlen("CALLBACK:")) == 0) {
			event.event.subscribe.callback = strdup(gena_trim(header + strlen("CALLBACK:")));
		} else if (strncasecmp(header, "SCOPE:", strlen("SCOPE:")) == 0) {
			event.event.subscribe.scope = strdup(gena_trim(header + strlen("SCOPE:")));
		} else if (strncasecmp(header, "TIMEOUT:", strlen("TIMEOUT:")) == 0) {
			event.event.subscribe.timeout = strdup(gena_trim(header + strlen("TIMEOUT:")));
		} else if (strncasecmp(header, "SID:", strlen("SID:")) == 0) {
			event.event.subscribe.sid = strdup(gena_trim(header + strlen("SID:")));
		}
	}

	debugf("subscribe event;\n"
	       "  path    : '%s'\n"
	       "  host    : '%s'\n"
	       "  nt      : '%s'\n"
	       "  callback: '%s'\n"
	       "  scope   : '%s'\n"
	       "  timeout : '%s'\n"
	       "  sid     : '%s'\n",
	       event.event.subscribe.path,
	       event.event.subscribe.host,
	       event.event.subscribe.nt,
	       event.event.subscribe.callback,
	       event.event.subscribe.scope,
	       event.event.subscribe.timeout,
	       event.event.subscribe.sid);

	if (event.event.subscribe.path == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	if (gena_thread->callbacks == NULL ||
	    gena_thread->callbacks->gena.event == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
		goto out;
	}
	if (event.event.subscribe.nt != NULL) {
		/* Subscription */
		if (strcasecmp(event.event.subscribe.nt, "upnp:event") != 0) {
			gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_PRECONDITION_FAILED);
			goto out;
		}
		if (event.event.subscribe.sid != NULL) {
			gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_BAD_REQUEST);
			goto out;
		}
		if (event.event.subscribe.callback == NULL) {
			gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_PRECONDITION_FAILED);
			goto out;
		}
		event.type = GENA_EVENT_TYPE_SUBSCRIBE_REQUEST;
		if (gena_thread->callbacks->gena.event(gena_thread->callbacks->gena.cookie, &event) == 0) {
			sprintf(header, fmt, event.event.subscribe.sid, 1800);
			debugf("header:\n%s", header);
			if (gena_send(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, strlen(header)) != strlen(header)) {
				debugf("send() failed");
			} else {
				event.type = GENA_EVENT_TYPE_SUBSCRIBE_ACCEPT;
				gena_thread->callbacks->gena.event(gena_thread->callbacks->gena.cookie, &event);
			}
			goto out;
		} else {
			gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
			goto out;
		}
	} else {
		/* Renewal */
		event.type = GENA_EVENT_TYPE_SUBSCRIBE_RENEW;
		if (gena_thread->callbacks->gena.event(gena_thread->callbacks->gena.cookie, &event) == 0) {
			sprintf(header, fmt, event.event.subscribe.sid, 1800);
			debugf("header:\n%s", header);
			if (gena_send(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, strlen(header)) != strlen(header)) {
				debugf("send() failed");
			} else {
				event.type = GENA_EVENT_TYPE_SUBSCRIBE_ACCEPT;
				gena_thread->callbacks->gena.event(gena_thread->callbacks->gena.cookie, &event);
			}
			goto out;
		} else {
			gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
			goto out;
		}
	}

	gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
out:
	free(event.event.subscribe.path);
	free(event.event.subscribe.host);
	free(event.event.subscribe.nt);
	free(event.event.subscribe.callback);
	free(event.event.subscribe.scope);
	free(event.event.subscribe.timeout);
	free(event.event.subscribe.sid);
}

static void gena_handler_post (gena_thread_t *gena_thread, char *header, const char *path)
{
	char *ptr;
	gena_event_t event;
	const char *format =
		"HTTP/1.1 200 OK\r\n"
		"CONTENT-LENGTH: %u\r\n"
		"CONTENT-TYPE: text/xml\r\n"
		"\r\n";

	memset(&event, 0, sizeof(gena_event_t));

	event.event.action.path = strdup(path);

	while (1) {
		if (gena_getline(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
			break;
		}
		if (strncasecmp(header, "HOST:", strlen("HOST:")) == 0) {
			event.event.action.host = strdup(gena_trim(header + strlen("HOST:")));
		} else if (strncasecmp(header, "CONTENT-LENGTH:", strlen("CONTENT-LENGTH:")) == 0) {
			event.event.action.length = atol(gena_trim(header + strlen("CONTENT-LENGTH:")));
		} else if (strncasecmp(header, "SOAPACTION:", strlen("SOAPACTION:")) == 0) {
			event.event.action.serviceid = strdup(gena_trim(header + strlen("SOAPACTION:")));
		}
	}
	if (event.event.action.length == 0 ||
	    event.event.action.host == NULL ||
	    event.event.action.serviceid == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_PRECONDITION_FAILED);
		goto out;
	}
	event.event.action.request = (char *) malloc(sizeof(char) * (event.event.action.length + 1));
	if (event.event.action.request == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	if (gena_getcontent(gena_thread->socket, GENA_SOCKET_TIMEOUT, event.event.action.request, event.event.action.length) != event.event.action.length) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}
	event.event.action.request[event.event.action.length] = '\0';

	ptr = strchr(event.event.action.serviceid, '#');
	if (ptr == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_PRECONDITION_FAILED);
		goto out;
	}
	*ptr++ = '\0';
	event.event.action.action = ptr;

	event.type = GENA_EVENT_TYPE_ACTION;

	debugf("post event;\n"
	       "  path: '%s'\n"
	       "  host: '%s'\n"
	       "  action: '%s'\n"
	       "  service: '%s'\n"
	       "  length: '%u'\n"
	       "  content: '%s'\n",
	       event.event.action.path,
	       event.event.action.host,
	       event.event.action.action,
	       event.event.action.serviceid,
	       event.event.action.length,
	       event.event.action.request);

	if (event.event.subscribe.path == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	if (gena_thread->callbacks == NULL ||
	    gena_thread->callbacks->gena.event == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
		goto out;
	}
	if (gena_thread->callbacks->gena.event(gena_thread->callbacks->gena.cookie, &event) == 0) {
		debugf("sending action response");
		sprintf(header, format, strlen(event.event.action.response));
		if (gena_send(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, strlen(header)) != strlen(header)) {
			debugf("send() failed");
		} else if (gena_send(gena_thread->socket, GENA_SOCKET_TIMEOUT, event.event.action.response, strlen(event.event.action.response)) != strlen(event.event.action.response)) {
			debugf("send() failed");
		}
		goto out;
	}
	gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);

out:
	free(event.event.action.serviceid);
	free(event.event.action.request);
	free(event.event.action.response);
	free(event.event.action.host);
	free(event.event.action.path);
}

static void * gena_thread_loop (void *arg)
{
	int running;
	gena_thread_t *gena_thread;

	char *data;
	char *header;
	int rangerequest;

	char *tmpptr;
	char *urlptr;
	char *pathptr;
	static const char request_get[] = "GET";
	static const char request_head[] = "HEAD";
	static const char request_post[] = "POST";
	static const char request_subscribe[] = "SUBSCRIBE";
	static const char request_unsubscribe[] = "UNSUBSCRIBE";

	int rlen;
	int readlen;
	void *filehandle;
	gena_fileinfo_internal_t fileinfo;

	gena_thread = (gena_thread_t *) arg;

	thread_mutex_lock(gena_thread->mutex);
	debugf("started gena child thread");
	running = 1;
	gena_thread->running = 1;
	thread_cond_signal(gena_thread->cond);
	thread_mutex_unlock(gena_thread->mutex);

	data = NULL;
	header = NULL;
	pathptr = NULL;
	rangerequest = 0;
	memset(&fileinfo, 0, sizeof(gena_fileinfo_internal_t));
	data = (char *) malloc(GENA_DATA_SIZE);
	if (data == NULL) {
		goto out;
	}
	header = (char *) malloc(GENA_HEADER_SIZE);
	if (header == NULL) {
		goto out;
	}
	if (gena_getline(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}

	/* parse method */
	urlptr = strpbrk(header, " \t");
	if (urlptr == NULL) {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}
	*urlptr++ = '\0';
	if (strcasecmp(header, request_get) != 0 &&
	    strcasecmp(header, request_head) != 0 &&
	    strcasecmp(header, request_post) != 0 &&
	    strcasecmp(header, request_subscribe) != 0 &&
	    strcasecmp(header, request_unsubscribe) != 0) {
		debugf("unsupported header: '%s'", header);
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
		goto out;
	}
	debugf("header: %s", header);
	while (*urlptr && (*urlptr == ' ' || *urlptr == '\t')) {
		urlptr++;
	}
	if (urlptr[0] != '/') {
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}
	tmpptr = urlptr;
	while (*tmpptr && (*tmpptr != ' ' && *tmpptr != '\t')) {
		tmpptr++;
	}
	*tmpptr = '\0';
	pathptr = strdup(urlptr);
	if (pathptr == NULL) {
		debugf("strdup(%s) failed", urlptr);
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	gena_remove_escaped_chars(pathptr);
	debugf("requested url is: %s", pathptr);

	if (strcasecmp(header, request_unsubscribe) == 0) {
		debugf("gena unsubscribe event");
		gena_handler_unsubscribe(gena_thread, header, pathptr);
		goto out;
	}
	if (strcasecmp(header, request_subscribe) == 0) {
		debugf("gena subscribe event");
		gena_handler_subscribe(gena_thread, header, pathptr);
		goto out;
	}
	if (strcasecmp(header, request_post) == 0) {
		debugf("gena post event");
		gena_handler_post(gena_thread, header, pathptr);
		goto out;
	}

	/* eat rest of headers */
	while (1) {
		if (gena_getline(gena_thread->socket, GENA_SOCKET_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
			break;
		}
		if (strncasecmp(header, "Range:", strlen("Range:")) == 0) {
			tmpptr = header + strlen("Range:");
			while (*tmpptr && *tmpptr == ' ') {
				tmpptr++;
			}
			if (strncmp(tmpptr, "bytes=", strlen("bytes=")) == 0) {
				rangerequest = 1;
				tmpptr += strlen("bytes=");
				fileinfo.filerange.start = strtoull(tmpptr, &tmpptr, 10);
				if (tmpptr[0] != '-' || fileinfo.filerange.start < 0) {
					fileinfo.filerange.start = 0;
					fileinfo.filerange.stop = 0;
					fileinfo.filerange.size = 0;
				} else if (tmpptr[1] != '\0') {
					fileinfo.filerange.stop = strtoull(tmpptr + 1, NULL, 10);
					if (fileinfo.filerange.stop < 0 || fileinfo.filerange.stop < fileinfo.filerange.start) {
						fileinfo.filerange.start = 0;
						fileinfo.filerange.stop = 0;
						fileinfo.filerange.size = 0;
					}
				}
			}
			debugf("range requested %lu-%lu", fileinfo.filerange.start, fileinfo.filerange.stop);
		}
	}

	/* do real job */
	if (gena_thread->callbacks == NULL ||
	    gena_thread->callbacks->vfs.info == NULL ||
	    gena_thread->callbacks->vfs.open == NULL ||
	    gena_thread->callbacks->vfs.read == NULL ||
	    gena_thread->callbacks->vfs.seek == NULL ||
	    gena_thread->callbacks->vfs.close == NULL) {
		debugf("no callbacks installed");
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}

	debugf("reading file info");
	if (gena_thread->callbacks->vfs.info(gena_thread->callbacks->vfs.cookie, pathptr, &fileinfo.fileinfo) < 0) {
		debugf("info failed");
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_NOT_FOUND);
		goto out;
	}
	debugf("opening file");
	filehandle = gena_thread->callbacks->vfs.open(gena_thread->callbacks->vfs.cookie, pathptr, GENA_FILEMODE_READ);
	if (filehandle == NULL) {
		debugf("open failed");
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}

	debugf("calculating actual size");
	/* calculate actual size */
	if (fileinfo.filerange.stop == 0 && rangerequest == 1) {
		/* we do support range as requested */
		fileinfo.filerange.stop = fileinfo.fileinfo.size - 1;
	}
	fileinfo.filerange.stop = MIN(fileinfo.filerange.stop, fileinfo.fileinfo.size - 1);
	if (fileinfo.filerange.start == 0 && fileinfo.filerange.stop == 0) {
		fileinfo.filerange.size = fileinfo.fileinfo.size;
	} else {
		fileinfo.filerange.size = fileinfo.filerange.stop - fileinfo.filerange.start + 1;
	}

	debugf("sending file header");
	/* send header */
	gena_sendfileheader(gena_thread->socket, &fileinfo);
	if (strcasecmp(header, request_head) == 0) {
		debugf("only header is requested");
		goto close_out;
	}

	debugf("seeking file");
	/* seek if requested */
	if (gena_thread->callbacks->vfs.seek(gena_thread->callbacks->vfs.cookie, filehandle, fileinfo.filerange.start, GENA_SEEK_SET) != fileinfo.filerange.start) {
		debugf("seek failed");
		gena_senderrorheader(gena_thread->socket, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto close_out;
	}

	debugf("sending file");
	/* send file */
	readlen = 0;
	while (readlen < fileinfo.filerange.size) {
		rlen = MIN(fileinfo.filerange.size - readlen, GENA_DATA_SIZE);
		rlen = gena_thread->callbacks->vfs.read(gena_thread->callbacks->vfs.cookie, filehandle, data, rlen);
		if (rlen < 0) {
			debugf("read failed");
			break;
		}
		if (gena_send(gena_thread->socket, GENA_SOCKET_TIMEOUT, data, rlen) != rlen) {
			debugf("send() failed");
			break;
		}
		thread_mutex_lock(gena_thread->mutex);
		if (gena_thread->running == 0) {
			thread_mutex_unlock(gena_thread->mutex);
			break;
		}
		thread_mutex_unlock(gena_thread->mutex);
		readlen += rlen;
	}

	if (readlen != fileinfo.filerange.size) {
		debugf("file send failed");
	}

close_out:
	debugf("closing file");
	gena_thread->callbacks->vfs.close(gena_thread->callbacks->vfs.cookie, filehandle);
out:
	free(pathptr);
	free(fileinfo.fileinfo.mimetype);
	free(data);
	free(header);
	thread_mutex_lock(gena_thread->mutex);
	debugf("stopped gena child thread");
	gena_thread->stopped = 1;
	thread_cond_signal(gena_thread->cond);
	thread_mutex_unlock(gena_thread->mutex);

	return NULL;
}

static void * gena_loop (void *arg)
{
	int rc;
	int running;
	int timeout;

	socket_t *socket;
	socket_event_t presult;

	gena_t *gena;
	gena_thread_t *gena_thread;
	gena_thread_t *gena_thread_next;

	gena = (gena_t *) arg;

	thread_mutex_lock(gena->mutex);
	debugf("started gena thread");
	running = 1;
	timeout = 500;
	gena->running = 1;
	thread_cond_signal(gena->cond);
	thread_mutex_unlock(gena->mutex);

	while (running == 1) {
		rc = socket_poll(gena->socket, SOCKET_EVENT_IN, &presult, timeout);

		thread_mutex_lock(gena->mutex);
		running = gena->running;
		list_for_each_entry_safe(gena_thread, gena_thread_next, &gena->threads, head) {
			thread_mutex_lock(gena_thread->mutex);
			if (gena_thread->stopped == 1) {
				thread_join(gena_thread->thread);
				debugf("closing stopped gena child threads");
				list_del(&gena_thread->head);
				thread_mutex_unlock(gena_thread->mutex);
				thread_mutex_destroy(gena_thread->mutex);
				thread_cond_destroy(gena_thread->cond);
				socket_close(gena_thread->socket);
				free(gena_thread);
			} else {
				thread_mutex_unlock(gena_thread->mutex);
			}
		}
		if (running == 0) {
			debugf("stopping and closing remaining gena child threads");
			list_for_each_entry_safe(gena_thread, gena_thread_next, &gena->threads, head) {
				thread_mutex_lock(gena_thread->mutex);
				if (gena_thread->stopped == 1) {
					thread_join(gena_thread->thread);
				} else if (gena_thread->running == 1) {
					gena_thread->running = 0;
					thread_cond_signal(gena_thread->cond);
					while (gena_thread->stopped != 1) {
						thread_cond_wait(gena_thread->cond, gena_thread->mutex);
					}
					thread_join(gena_thread->thread);
				} else {
					debugf("this should not happen");
					assert(0);
				}
				list_del(&gena_thread->head);
				thread_mutex_unlock(gena_thread->mutex);
				thread_mutex_destroy(gena_thread->mutex);
				thread_cond_destroy(gena_thread->cond);
				socket_close(gena_thread->socket);
				free(gena_thread);
			}
		}
		thread_mutex_unlock(gena->mutex);

		if (running == 0 || rc <= 0 || presult != SOCKET_EVENT_IN) {
			continue;
		}

		debugf("we have a new connection request");
		socket = socket_accept(gena->socket);
		if (socket != NULL) {
			debugf("accepted new connection");
			gena_thread = (gena_thread_t *) malloc(sizeof(gena_thread_t));
			if (gena_thread == NULL) {
				debugf("malloc(sizeof(gena_thread_t) failed");
				socket_close(socket);
				continue;
			}
			memset(gena_thread, 0, sizeof(gena_thread_t));
			gena_thread->socket = socket;
			gena_thread->callbacks = gena->callbacks;
			gena_thread->mutex = thread_mutex_init("gena_thread->mutex", 0);
			gena_thread->cond = thread_cond_init("gena_thread->cond");
			thread_mutex_lock(gena_thread->mutex);
			gena_thread->thread = thread_create("gena_thread_loop", gena_thread_loop, gena_thread);
			while (gena_thread->running != 1) {
				thread_cond_wait(gena_thread->cond, gena_thread->mutex);
			}
			list_add(&gena_thread->head, &gena->threads);
			debugf("created and added new gena thread");
			thread_mutex_unlock(gena_thread->mutex);
		}
	}

	thread_mutex_lock(gena->mutex);
	debugf("stopped gena thread");
	gena->stopped = 1;
	thread_cond_signal(gena->cond);
	thread_mutex_unlock(gena->mutex);

	return NULL;
}

static int gena_init_server (gena_t *gena)
{
	socket_t *socket;
	unsigned short port;

	port = gena->port;
	if (port < GENA_LISTEN_PORT) {
		port = GENA_LISTEN_PORT;
	}

	socket = socket_open(SOCKET_TYPE_STREAM);
	if (socket == NULL) {
		debugf("socket() failed");
		return -1;
	}
	do {
		port++;
		debugf("trying port: %d", port);
		if (socket_bind(socket, gena->address, port) != -1) {
			break;
		}
	} while (1);

	if (socket_listen(socket, GENA_LISTEN_MAX) < 0) {
		debugf("listen(%d) failed", GENA_LISTEN_MAX);
		socket_close(socket);
		return -1;
	}

	gena->socket = socket;
	gena->port = port;
	debugf("gena started on port: %s:%u", gena->address, gena->port);
	return 0;
}

char * gena_send_recv (gena_t *gena, const char *host, const unsigned short port, const char *header, const char *data)
{
	int r;
	int t;
	char *tmp;
	char *buffer;
	int contentlength;
	unsigned int length;

	socket_t *socket;
	socket_event_t presult;

	socket = socket_open(SOCKET_TYPE_STREAM);
	if (socket == NULL) {
		return NULL;
	}
	if (gena_connect(socket, GENA_SOCKET_TIMEOUT, host, port) != 0) {
		socket_close(socket);
		return NULL;
	}
	if (gena_send(socket, GENA_SOCKET_TIMEOUT, header, strlen(header)) != strlen(header)) {
		socket_close(socket);
		debugf("gena_send() failed");
		return NULL;
	}
	if (data != NULL) {
		if (gena_send(socket, GENA_SOCKET_TIMEOUT, data, strlen(data)) != strlen(data)) {
			socket_close(socket);
			debugf("gena_send() failed");
			return NULL;
		}
	}
	length = 0;
	contentlength = 0;
	buffer = malloc(GENA_HEADER_SIZE);
	while (1) {
		if (gena_getline(socket, GENA_SOCKET_TIMEOUT, buffer, GENA_HEADER_SIZE) <= 0) {
			break;
		}
		if (strncasecmp(buffer, "Content-length:", strlen("Content-length:")) == 0) {
			length = atol(gena_trim(buffer + strlen("Content-length:")));
			contentlength = 1;
		}
	}
	free(buffer);
	buffer = NULL;
	if (contentlength == 1) {
		if (length <= 0) {
			socket_close(socket);
			debugf("no data received %d\n", length);
			return NULL;
		}
		buffer = malloc(sizeof(char) * (length + 1));
		if (buffer == NULL) {
			socket_close(socket);
			debugf("no data received %d", length);
			return NULL;
		}
		t = 0;
		while (t < length) {
			r = socket_poll(socket, SOCKET_EVENT_IN, &presult, GENA_SOCKET_TIMEOUT);
			if (r <= 0 || (presult & SOCKET_EVENT_IN) == 0) {
				break;
			}
			r = socket_recv(socket, buffer + t, length - t);
			if (r <= 0) {
				break;
			}
			t += r;
		}
		buffer[length] = '\0';
		if (t != length) {
			free(buffer);
			buffer = NULL;
		}
	} else {
		length = 0;
		buffer = NULL;
again:
		length += 512;
		tmp = realloc(buffer, sizeof(char) * (length + 1));
		if (tmp == NULL) {
			socket_close(socket);
			free(buffer);
			debugf("no data received %d", length);
			return NULL;
		}
		buffer = tmp;
		t = length - 512;
		while (t < length) {
			r = socket_poll(socket, SOCKET_EVENT_IN, &presult, GENA_SOCKET_TIMEOUT);
			if (r <= 0 || (presult & SOCKET_EVENT_IN) == 0) {
				goto done;
				break;
			}
			r = socket_recv(socket, buffer + t, length - t);
			if (r <= 0) {
				goto done;
			}
			t += r;
			buffer[t] = '\0';
		}
		goto again;
done:
		buffer[t] = '\0';
	}
	socket_close(socket);
	return buffer;
}

char * gena_download (gena_t *gena, const char *host, const unsigned short port, const char *path)
{
	char *data;
	char *buffer;
	char *format_get =
		"GET /%s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Accept: */*\r\n"
		"Connection: keep-alive\r\n"
		"Cache-Control: max-age=0\r\n"
		"\r\n";
	debugf("downloading '/%s' from '%s:%u'", path, host, port);
	if (asprintf(&buffer, format_get, path, host, port) < 0) {
		return NULL;
	}
	data = gena_send_recv(gena, host, port, buffer, NULL);
	free(buffer);
	return data;
}

unsigned short gena_getport (gena_t *gena)
{
	return gena->port;
}

const char * gena_getaddress (gena_t *gena)
{
	return gena->address;
}

gena_t * gena_init (char *address, unsigned short port, gena_callbacks_t *callbacks)
{
	gena_t *gena;
	gena = (gena_t *) malloc(sizeof(gena_t));
	if (gena == NULL) {
		debugf("malloc(sizeof(gena_t)) failed");
		return NULL;
	}
	memset(gena, 0, sizeof(gena_t));
	gena->address = (address != NULL) ? strdup(address) : NULL;
	gena->port = port;
	gena->callbacks = callbacks;
	list_init(&gena->threads);
	gena_init_server(gena);

	gena->mutex = thread_mutex_init("gena->mutex", 0);
	gena->cond = thread_cond_init("gena->cond");
	gena->thread = thread_create("gena_loop", gena_loop, gena);

	thread_mutex_lock(gena->mutex);
	while (gena->running == 0) {
		thread_cond_wait(gena->cond, gena->mutex);
	}
	debugf("started gena loop");
	thread_mutex_unlock(gena->mutex);
	debugf("initialized gena");
	return gena;
}

int gena_uninit (gena_t *gena)
{
	debugf("stopping gena thread");
	if (gena == NULL) {
		goto out;
	}
	thread_mutex_lock(gena->mutex);
	gena->running = 0;
	thread_cond_signal(gena->cond);
	while (gena->stopped != 1) {
		thread_cond_wait(gena->cond, gena->mutex);
	}
	thread_join(gena->thread);
	thread_mutex_unlock(gena->mutex);
	thread_mutex_destroy(gena->mutex);
	thread_cond_destroy(gena->cond);
	socket_close(gena->socket);
	free(gena->address);
	free(gena);
out:	debugf("gena uninited");
	return 0;
}
