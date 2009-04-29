/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 by Alper Akcan
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>

#include "upnp.h"
#include "common.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))
#define WEBSERVER_LISTEN_PORT	10000
#define WEBSERVER_LISTEN_MAX	100
#define WEBSERVER_HEADER_SIZE	(1024 * 4)
#define WEBSERVER_DATA_SIZE	(1024 * 1024)
#define WEBSERVER_READ_TIMEOUT	1000
#define WEBSERVER_SEND_TIMEOUT	-1

typedef struct webserver_filerange_s {
	unsigned long start;
	unsigned long stop;
	unsigned long size;
} webserver_filerange_t;

typedef struct webserver_fileino_internal_s {
	webserver_fileinfo_t fileinfo;
	webserver_filerange_t filerange;
} webserver_fileinfo_internal_t;

typedef struct webserver_thread_s {
	list_t head;
	int running;
	int stopped;
	int fd;
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	webserver_callbacks_t *callbacks;
} webserver_thread_t;

typedef enum {
	WEBSERVER_RESPONSE_TYPE_OK,
	WEBSERVER_RESPONSE_TYPE_PARTIAL_CONTENT,
	WEBSERVER_RESPONSE_TYPE_BAD_REQUEST,
	WEBSERVER_RESPONSE_TYPE_INTERNAL_SERVER_ERROR,
	WEBSERVER_RESPONSE_TYPE_NOT_FOUND,
	WEBSERVER_RESPONSE_TYPE_NOT_IMPLEMENTED,
	WEBSERVER_RESPONSE_TYPES,
} webserver_response_type_t;

typedef struct webserver_response_s {
	webserver_response_type_t type;
	int code;
	char *name;
	char *info;
} webserver_response_t;

static const webserver_response_t webserver_responses[] = {
	{WEBSERVER_RESPONSE_TYPE_OK, 200, "OK", NULL},
	{WEBSERVER_RESPONSE_TYPE_PARTIAL_CONTENT, 206, "Partial Content", NULL},
	{WEBSERVER_RESPONSE_TYPE_BAD_REQUEST, 400, "Bad Request", "Unsupported method"},
	{WEBSERVER_RESPONSE_TYPE_INTERNAL_SERVER_ERROR, 500, "Internal Server Error", "Internal Server Error"},
	{WEBSERVER_RESPONSE_TYPE_NOT_FOUND, 404, "Not Found", "The requested URL was not found"},
	{WEBSERVER_RESPONSE_TYPE_NOT_IMPLEMENTED, 501, "Not implemented", "Not implemented"},
};

static int webserver_send (int fd, int timeout, const void *buf, unsigned int len)
{
	int rc;
	struct pollfd pfd;

	memset(&pfd, 0, sizeof(struct pollfd));
	pfd.fd = fd;
	pfd.events = POLLOUT;

	rc = poll(&pfd, 1, timeout);
	if (rc <= 0 || pfd.revents != POLLOUT) {
		return 0;
	}

	return send(pfd.fd, buf, len, 0);
}

static int webserver_getline (int fd, int timeout, char *buf, int buflen)
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
			debugf("%s", buf);
			return count;
		}
		if (count < buflen - 1) {
			count++;
		}
	}
	return count;
}

static int webserver_replace_escaped (char *in, int index, size_t *max)
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

static int webserver_remove_escaped_chars (char *in)
{
	int i = 0;
	size_t size;
	size = strlen(in);
	for( i = 0; i < size; i++ ) {
		if (webserver_replace_escaped(in, i, &size) != 0) {
			i--;
		}
	}
	return 0;
}

static void webserver_senderrorheader (int fd, webserver_response_type_t type)
{
	static const char RFC1123FMT[] = "%a, %d %b %Y %H:%M:%S GMT";

	int len;
	char *header;
	time_t timer;
	unsigned int i;
	char tmpstr[80];
	int responseNum = 0;
	const char *mimetype = NULL;
	const char *infoString = NULL;
	const char *responseString = "";

	timer = time(0);
	header = (char *) malloc(WEBSERVER_HEADER_SIZE);
	if (header == NULL) {
		return;
	}

	for (i = 0; i < ARRAY_SIZE(webserver_responses); i++) {
		if (webserver_responses[i].type == type) {
			responseNum = webserver_responses[i].code;
			responseString = webserver_responses[i].name;
			infoString = webserver_responses[i].info;
			break;
		}
	}
	mimetype = "text/html";

	strftime(tmpstr, sizeof(tmpstr), RFC1123FMT, gmtime(&timer));
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
	if (webserver_send(fd, WEBSERVER_SEND_TIMEOUT, header, len) != len) {
		debugf("send() failed");
	}
	free(header);
}

static void webserver_sendfileheader (int fd, webserver_fileinfo_internal_t *fileinfo)
{
	static const char RFC1123FMT[] = "%a, %d %b %Y %H:%M:%S GMT";

	int len;
	char *header;
	time_t timer;
	unsigned int i;
	char tmpstr[80];
	int responseNum = 0;
	const char *infoString = NULL;
	const char *responseString = "";

	webserver_response_type_t type;

	timer = time(0);
	header = (char *) malloc(WEBSERVER_HEADER_SIZE);
	if (header == NULL) {
		return;
	}

	if (fileinfo->filerange.start == 0 && fileinfo->filerange.stop == 0) {
		type = WEBSERVER_RESPONSE_TYPE_OK;
	} else {
		type = WEBSERVER_RESPONSE_TYPE_PARTIAL_CONTENT;
	}

	for (i = 0; i < ARRAY_SIZE(webserver_responses); i++) {
		if (webserver_responses[i].type == type) {
			responseNum = webserver_responses[i].code;
			responseString = webserver_responses[i].name;
			infoString = webserver_responses[i].info;
			break;
		}
	}

	strftime(tmpstr, sizeof(tmpstr), RFC1123FMT, gmtime(&timer));
	len = sprintf(header, "HTTP/1.0 %d %s\r\nContent-type: %s\r\n"
			      "Date: %s\r\nConnection: close\r\n",
			      responseNum, responseString, fileinfo->fileinfo.mimetype, tmpstr);

	strftime(tmpstr, sizeof(tmpstr), RFC1123FMT, gmtime((time_t *) &fileinfo->fileinfo.mtime));
	if (type == WEBSERVER_RESPONSE_TYPE_PARTIAL_CONTENT) {
		len += sprintf(header + len,
			"Accept-Ranges: bytes\r\n"
			"Last-Modified: %s\r\n%s %lu\r\n",
			tmpstr,
			"Content-length:",
			fileinfo->filerange.size);
		len += sprintf(header + len,
				"Content-Range: bytes %lu-%lu/%lu\r\n",
				fileinfo->filerange.start,
				fileinfo->filerange.stop,
				fileinfo->fileinfo.size);
	} else {
		len += sprintf(header + len,
			"Accept-Ranges: bytes\r\n"
			"Last-Modified: %s\r\n%s %lu\r\n",
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
	if (webserver_send(fd, WEBSERVER_SEND_TIMEOUT, header, len) != len) {
		debugf("send() failed");
	}
	free(header);
}

static void * webserver_thread_loop (void *arg)
{
	int running;
	webserver_thread_t *webserver_thread;

	char *data;
	char *header;
	int rangerequest;

	char *tmpptr;
	char *urlptr;
	char *pathptr;
	static const char request_get[] = "GET";
	static const char request_head[] = "HEAD";

	int rlen;
	int readlen;
	void *filehandle;
	webserver_fileinfo_internal_t fileinfo;

	webserver_thread = (webserver_thread_t *) arg;

	pthread_mutex_lock(&webserver_thread->mutex);
	debugf("started webserver child thread");
	running = 1;
	webserver_thread->running = 1;
	pthread_cond_signal(&webserver_thread->cond);
	pthread_mutex_unlock(&webserver_thread->mutex);

	data = NULL;
	header = NULL;
	pathptr = NULL;
	rangerequest = 0;
	memset(&fileinfo, 0, sizeof(webserver_fileinfo_internal_t));
	data = (char *) malloc(WEBSERVER_DATA_SIZE);
	if (data == NULL) {
		goto out;
	}
	header = (char *) malloc(WEBSERVER_HEADER_SIZE);
	if (header == NULL) {
		goto out;
	}
	if (webserver_getline(webserver_thread->fd, WEBSERVER_READ_TIMEOUT, header, WEBSERVER_HEADER_SIZE) <= 0) {
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}

	/* parse method */
	urlptr = strpbrk(header, " \t");
	if (urlptr == NULL) {
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}
	*urlptr++ = '\0';
	if (strcasecmp(header, request_get) != 0 &&
	    strcasecmp(header, request_head) != 0) {
		debugf("support get/head request only (%s)", header);
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_NOT_IMPLEMENTED);
		goto out;
	}
	while (*urlptr && *urlptr == ' ') {
		urlptr++;
	}
	if (urlptr[0] != '/') {
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}
	tmpptr = urlptr;
	while (*tmpptr && *tmpptr != ' ') {
		tmpptr++;
	}
	*tmpptr = '\0';
	pathptr = strdup(urlptr);
	if (pathptr == NULL) {
		debugf("strdup(%s) failed", urlptr);
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	webserver_remove_escaped_chars(pathptr);
	debugf("requested url is: %s", pathptr);

	/* eat rest of headers */
	while (1) {
		if (webserver_getline(webserver_thread->fd, WEBSERVER_READ_TIMEOUT, header, WEBSERVER_HEADER_SIZE) <= 0) {
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
				fileinfo.filerange.start = strtoul(tmpptr, &tmpptr, 10);
				if (tmpptr[0] != '-' || fileinfo.filerange.start < 0) {
					fileinfo.filerange.start = 0;
					fileinfo.filerange.stop = 0;
					fileinfo.filerange.size = 0;
				} else if (tmpptr[1] != '\0') {
					fileinfo.filerange.stop = strtoul(tmpptr + 1, NULL, 10);
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
	if (webserver_thread->callbacks == NULL ||
	    webserver_thread->callbacks->info == NULL ||
	    webserver_thread->callbacks->open == NULL ||
	    webserver_thread->callbacks->read == NULL ||
	    webserver_thread->callbacks->seek == NULL ||
	    webserver_thread->callbacks->close == NULL) {
		debugf("no callbacks installed");
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}

	if (webserver_thread->callbacks->info(webserver_thread->callbacks->cookie, pathptr, &fileinfo.fileinfo) < 0) {
		debugf("info failed");
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_NOT_FOUND);
		goto out;
	}
	filehandle = webserver_thread->callbacks->open(webserver_thread->callbacks->cookie, pathptr, WEBSERVER_FILEMODE_READ);
	if (filehandle == NULL) {
		debugf("open failed");
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}

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

	/* send header */
	webserver_sendfileheader(webserver_thread->fd, &fileinfo);
	if (strcasecmp(header, request_head) == 0) {
		debugf("only header is requested");
		goto close_out;
	}

	/* seek if requested */
	if (webserver_thread->callbacks->seek(webserver_thread->callbacks->cookie, filehandle, fileinfo.filerange.start, WEBSERVER_SEEK_SET) != fileinfo.filerange.start) {
		debugf("seek failed");
		webserver_senderrorheader(webserver_thread->fd, WEBSERVER_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto close_out;
	}

	/* send file */
	readlen = 0;
	while (readlen < fileinfo.filerange.size) {
		rlen = MIN(fileinfo.filerange.size - readlen, WEBSERVER_DATA_SIZE);
		rlen = webserver_thread->callbacks->read(webserver_thread->callbacks->cookie, filehandle, data, rlen);
		if (rlen < 0) {
			debugf("read failed");
			break;
		}
		if (webserver_send(webserver_thread->fd, WEBSERVER_SEND_TIMEOUT, data, rlen) != rlen) {
			debugf("send() failed");
			break;
		}
		pthread_mutex_lock(&webserver_thread->mutex);
		if (webserver_thread->running == 0) {
			pthread_mutex_unlock(&webserver_thread->mutex);
			break;
		}
		pthread_mutex_unlock(&webserver_thread->mutex);
		readlen += rlen;
	}

	if (readlen != fileinfo.filerange.size) {
		debugf("file send failed");
	}

close_out:
	webserver_thread->callbacks->close(webserver_thread->callbacks->cookie, filehandle);
out:
	free(pathptr);
	free(fileinfo.fileinfo.mimetype);
	free(data);
	free(header);
	pthread_mutex_lock(&webserver_thread->mutex);
	debugf("stopped webserver child thread");
	webserver_thread->stopped = 1;
	pthread_cond_signal(&webserver_thread->cond);
	pthread_mutex_unlock(&webserver_thread->mutex);

	return NULL;
}

static void * webserver_loop (void *arg)
{
	int fd;
	int rc;
	int running;
	int timeout;
	struct pollfd pfd;
	socklen_t client_len;
	struct sockaddr_in client;

	webserver_t *webserver;
	webserver_thread_t *webserver_thread;
	webserver_thread_t *webserver_thread_next;

	webserver = (webserver_t *) arg;

	pthread_mutex_lock(&webserver->mutex);
	debugf("started webserver thread");
	running = 1;
	timeout = 500;
	webserver->running = 1;
	pthread_cond_signal(&webserver->cond);
	pthread_mutex_unlock(&webserver->mutex);

	while (running == 1) {
		memset(&pfd, 0, sizeof(struct pollfd));

		pfd.fd = webserver->fd;
		pfd.events = POLLIN;

		rc = poll(&pfd, 1, timeout);

		pthread_mutex_lock(&webserver->mutex);
		running = webserver->running;
		list_for_each_entry_safe(webserver_thread, webserver_thread_next, &webserver->threads, head) {
			pthread_mutex_lock(&webserver_thread->mutex);
			if (webserver_thread->stopped == 1) {
				pthread_join(webserver_thread->thread, NULL);
				debugf("closing stopped webserver child threads");
				list_del(&webserver_thread->head);
				pthread_mutex_destroy(&webserver_thread->mutex);
				pthread_cond_destroy(&webserver_thread->cond);
				close(webserver_thread->fd);
				free(webserver_thread);
			} else {
				pthread_mutex_unlock(&webserver_thread->mutex);
			}
		}
		if (running == 0) {
			debugf("stopping and closing remaining webserver child threads");
			list_for_each_entry_safe(webserver_thread, webserver_thread_next, &webserver->threads, head) {
				pthread_mutex_lock(&webserver_thread->mutex);
				if (webserver_thread->stopped == 1) {
					pthread_join(webserver_thread->thread, NULL);
				} else if (webserver_thread->running == 1) {
					webserver_thread->running = 0;
					pthread_cond_signal(&webserver_thread->cond);
					while (webserver_thread->stopped != 1) {
						pthread_cond_wait(&webserver_thread->cond, &webserver_thread->mutex);
					}
					pthread_join(webserver_thread->thread, NULL);
				} else {
					debugf("this should not happen");
					assert(0);
				}
				list_del(&webserver_thread->head);
				pthread_mutex_unlock(&webserver_thread->mutex);
				pthread_mutex_destroy(&webserver_thread->mutex);
				pthread_cond_destroy(&webserver_thread->cond);
				close(webserver_thread->fd);
				free(webserver_thread);
			}
		}
		pthread_mutex_unlock(&webserver->mutex);

		if (running == 0 || rc <= 0 || pfd.revents != POLLIN) {
			continue;
		}

		debugf("we have a new connection request");
		memset(&client, 0, sizeof(client));
		client_len = sizeof(client);
		fd = accept(pfd.fd, (struct sockaddr*) &client, &client_len);
		if (fd >= 0) {
			debugf("accepted new connection");
			webserver_thread = (webserver_thread_t *) malloc(sizeof(webserver_thread_t));
			if (webserver_thread == NULL) {
				debugf("malloc(sizeof(webserver_thread_t) failed");
				close(fd);
				continue;
			}
			memset(webserver_thread, 0, sizeof(webserver_thread_t));
			webserver_thread->fd = fd;
			webserver_thread->callbacks = webserver->callbacks;
			pthread_mutex_init(&webserver_thread->mutex, NULL);
			pthread_cond_init(&webserver_thread->cond, NULL);
			pthread_mutex_lock(&webserver_thread->mutex);
			pthread_create(&webserver_thread->thread, NULL, webserver_thread_loop, webserver_thread);
			while (webserver_thread->running != 1) {
				pthread_cond_wait(&webserver_thread->cond, &webserver_thread->mutex);
			}
			list_add(&webserver_thread->head, &webserver->threads);
			debugf("created and added new webserver thread");
			pthread_mutex_unlock(&webserver_thread->mutex);
		}
	}

	pthread_mutex_lock(&webserver->mutex);
	debugf("stopped webserver thread");
	webserver->stopped = 1;
	pthread_cond_signal(&webserver->cond);
	pthread_mutex_unlock(&webserver->mutex);

	return NULL;
}

static int webserver_init_server (webserver_t *webserver)
{
	int fd;
	socklen_t len;
	unsigned short port;
	struct sockaddr_in soc;

	port = webserver->port;
	if (port < WEBSERVER_LISTEN_PORT) {
		port = WEBSERVER_LISTEN_PORT;
	}

	len = sizeof(soc);
	memset(&soc, 0, len);
        soc.sin_family = AF_INET;
	soc.sin_addr.s_addr = inet_addr(webserver->address);
	soc.sin_port = htons(port);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		debugf("socket() failed");
		return -1;
	}
	do {
		soc.sin_port = htons(port++);
		if (bind(fd, (struct sockaddr *) &soc, len) != -1) {
			break;
		}
		debugf("trying port: %d", port);
	} while (1);

	if (listen(fd, WEBSERVER_LISTEN_MAX) < 0) {
		debugf("listen(%d) failed", WEBSERVER_LISTEN_MAX);
		close(fd);
		return -1;
	}

	if (getsockname(fd, (struct sockaddr *) &soc, &len) < 0) {
		close(fd);
		return -1;
	}
	webserver->fd = fd;
	webserver->port = ntohs(soc.sin_port);
	debugf("webserver started on port: %s:%u", inet_ntoa(soc.sin_addr), webserver->port);

	return 0;
}

unsigned short webserver_getport (webserver_t *webserver)
{
	return webserver->port;
}

const char * webserver_getaddress (webserver_t *webserver)
{
	return webserver->address;
}

webserver_t * webserver_init (char *address, unsigned short port, webserver_callbacks_t *callbacks)
{
	webserver_t *webserver;
	webserver = (webserver_t *) malloc(sizeof(webserver_t));
	if (webserver == NULL) {
		debugf("malloc(sizeof(webserver_t)) failed");
		return NULL;
	}
	memset(webserver, 0, sizeof(webserver_t));
	webserver->address = (address != NULL) ? strdup(address) : NULL;
	webserver->port = port;
	webserver->callbacks = callbacks;
	list_init(&webserver->threads);
	webserver_init_server(webserver);

	pthread_mutex_init(&webserver->mutex, NULL);
	pthread_cond_init(&webserver->cond, NULL);
	pthread_create(&webserver->thread, NULL, webserver_loop, webserver);

	pthread_mutex_lock(&webserver->mutex);
	while (webserver->running == 0) {
		pthread_cond_wait(&webserver->cond, &webserver->mutex);
	}
	debugf("started webserver loop");
	pthread_mutex_unlock(&webserver->mutex);
	debugf("initialized webserver");
	return webserver;
}

int webserver_uninit (webserver_t *webserver)
{
	debugf("stopping webserver thread");
	if (webserver == NULL) {
		goto out;
	}
	pthread_mutex_lock(&webserver->mutex);
	webserver->running = 0;
	pthread_cond_signal(&webserver->cond);
	while (webserver->stopped != 1) {
		pthread_cond_wait(&webserver->cond, &webserver->mutex);
	}
	pthread_join(webserver->thread, NULL);
	pthread_mutex_unlock(&webserver->mutex);
	pthread_mutex_destroy(&webserver->mutex);
	pthread_cond_destroy(&webserver->cond);
	close(webserver->fd);
	free(webserver->address);
	free(webserver);
out:	debugf("webserver uninited");
	return 0;
}
