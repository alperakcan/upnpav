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
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

typedef struct list_s list_t;

struct list_s {
	struct list_s *next;
	struct list_s *prev;
};

#define offsetof_(type, member) ((size_t) &((type *) 0)->member)

#define containerof_(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof_(type,member) );})

#define list_entry(ptr, type, member) \
	containerof_(ptr, type, member)

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
	     n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

static inline int list_count (list_t *head)
{
	int count;
	list_t *entry;
	count = 0;
	list_for_each(entry, head) {
		count++;
	}
	return count;
}

static inline void list_add_actual (list_t *new, list_t *prev, list_t *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_del_actual (list_t *prev, list_t *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_add_tail (list_t *new, list_t *head)
{
	list_add_actual(new, head->prev, head);
}

static inline void list_add (list_t *new, list_t *head)
{
	list_add_actual(new, head, head->next);
}

static inline void list_del (list_t *entry)
{
	list_del_actual(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

static inline int list_is_first (list_t *list, list_t *head)
{
	return head->next == list;
}

static inline int list_is_last (list_t *list, list_t *head)
{
	return list->next == head;
}

static inline void list_init (list_t *head)
{
	head->next = head;
	head->prev = head;
}

#define debugf(a...) { \
	printf(a); \
	printf(" [%s (%s:%d)]\n", __FUNCTION__, __FILE__, __LINE__); \
}

#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))
#define GENA_LISTEN_PORT	10000
#define GENA_LISTEN_MAX	100
#define GENA_HEADER_SIZE	(1024 * 4)
#define GENA_DATA_SIZE	(1024 * 1024)
#define GENA_READ_TIMEOUT	1000
#define GENA_SEND_TIMEOUT	-1

typedef struct gena_s {
	int running;
	int stopped;
	int fd;
	char *address;
	unsigned short port;
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	list_t threads;
} gena_t;

typedef struct gena_thread_s {
	list_t head;
	int running;
	int stopped;
	int fd;
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} gena_thread_t;

typedef enum {
	GENA_RESPONSE_TYPE_OK,
	GENA_RESPONSE_TYPE_PARTIAL_CONTENT,
	GENA_RESPONSE_TYPE_BAD_REQUEST,
	GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR,
	GENA_RESPONSE_TYPE_NOT_FOUND,
	GENA_RESPONSE_TYPE_NOT_IMPLEMENTED,
	GENA_RESPONSE_TYPES,
} gena_response_type_t;

typedef struct gena_response_s {
	gena_response_type_t type;
	int code;
	char *name;
	char *info;
} gena_response_t;

static const gena_response_t gena_responses[] = {
	{GENA_RESPONSE_TYPE_OK, 200, "OK", NULL},
	{GENA_RESPONSE_TYPE_PARTIAL_CONTENT, 206, "Partial Content", NULL},
	{GENA_RESPONSE_TYPE_BAD_REQUEST, 400, "Bad Request", "Unsupported method"},
	{GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR, 500, "Internal Server Error", "Internal Server Error"},
	{GENA_RESPONSE_TYPE_NOT_FOUND, 404, "Not Found", "The requested URL was not found"},
	{GENA_RESPONSE_TYPE_NOT_IMPLEMENTED, 501, "Not implemented", "Not implemented"},
};

static int gena_send (int fd, int timeout, const void *buf, unsigned int len)
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

static int gena_getline (int fd, int timeout, char *buf, int buflen)
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
	unsigned int size;
	size = strlen(in);
	for( i = 0; i < size; i++ ) {
		if (gena_replace_escaped(in, i, &size) != 0) {
			i--;
		}
	}
	return 0;
}

static void gena_senderrorheader (int fd, gena_response_type_t type)
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
	if (gena_send(fd, GENA_SEND_TIMEOUT, header, len) != len) {
		debugf("send() failed");
	}
	free(header);
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

	gena_thread = (gena_thread_t *) arg;

	pthread_mutex_lock(&gena_thread->mutex);
	debugf("started gena child thread");
	running = 1;
	gena_thread->running = 1;
	pthread_cond_signal(&gena_thread->cond);
	pthread_mutex_unlock(&gena_thread->mutex);

	data = NULL;
	header = NULL;
	pathptr = NULL;
	rangerequest = 0;
	data = (char *) malloc(GENA_DATA_SIZE);
	if (data == NULL) {
		goto out;
	}
	header = (char *) malloc(GENA_HEADER_SIZE);
	if (header == NULL) {
		goto out;
	}
	if (gena_getline(gena_thread->fd, GENA_READ_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
		gena_senderrorheader(gena_thread->fd, GENA_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}

	/* parse method */
	urlptr = strpbrk(header, " \t");
	if (urlptr == NULL) {
		gena_senderrorheader(gena_thread->fd, GENA_RESPONSE_TYPE_BAD_REQUEST);
		goto out;
	}
	*urlptr++ = '\0';
	debugf("header request: %s", header);
	if (strcasecmp(header, request_get) != 0 &&
	    strcasecmp(header, request_head) != 0) {
		debugf("support get/head request only (%s)", header);
		gena_senderrorheader(gena_thread->fd, GENA_RESPONSE_TYPE_NOT_IMPLEMENTED);
		goto out;
	}
	while (*urlptr && *urlptr == ' ') {
		urlptr++;
	}
	if (urlptr[0] != '/') {
		gena_senderrorheader(gena_thread->fd, GENA_RESPONSE_TYPE_BAD_REQUEST);
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
		gena_senderrorheader(gena_thread->fd, GENA_RESPONSE_TYPE_INTERNAL_SERVER_ERROR);
		goto out;
	}
	gena_remove_escaped_chars(pathptr);
	debugf("requested url is: %s", pathptr);

	/* eat rest of headers */
	while (1) {
		if (gena_getline(gena_thread->fd, GENA_READ_TIMEOUT, header, GENA_HEADER_SIZE) <= 0) {
			break;
		}
	}

out:
	free(pathptr);
	free(data);
	free(header);
	pthread_mutex_lock(&gena_thread->mutex);
	debugf("stopped gena child thread");
	gena_thread->stopped = 1;
	pthread_cond_signal(&gena_thread->cond);
	pthread_mutex_unlock(&gena_thread->mutex);

	return NULL;
}

static void * gena_loop (void *arg)
{
	int fd;
	int rc;
	int running;
	int timeout;
	struct pollfd pfd;
	socklen_t client_len;
	struct sockaddr_in client;

	gena_t *gena;
	gena_thread_t *gena_thread;
	gena_thread_t *gena_thread_next;

	gena = (gena_t *) arg;

	pthread_mutex_lock(&gena->mutex);
	debugf("started gena thread");
	running = 1;
	timeout = 500;
	gena->running = 1;
	pthread_cond_signal(&gena->cond);
	pthread_mutex_unlock(&gena->mutex);

	while (running == 1) {
		memset(&pfd, 0, sizeof(struct pollfd));

		pfd.fd = gena->fd;
		pfd.events = POLLIN;

		rc = poll(&pfd, 1, timeout);

		pthread_mutex_lock(&gena->mutex);
		running = gena->running;
		list_for_each_entry_safe(gena_thread, gena_thread_next, &gena->threads, head) {
			pthread_mutex_lock(&gena_thread->mutex);
			if (gena_thread->stopped == 1) {
				pthread_join(gena_thread->thread, NULL);
				debugf("closing stopped gena child threads");
				list_del(&gena_thread->head);
				pthread_mutex_destroy(&gena_thread->mutex);
				pthread_cond_destroy(&gena_thread->cond);
				close(gena_thread->fd);
				free(gena_thread);
			} else {
				pthread_mutex_unlock(&gena_thread->mutex);
			}
		}
		if (running == 0) {
			debugf("stopping and closing remaining gena child threads");
			list_for_each_entry_safe(gena_thread, gena_thread_next, &gena->threads, head) {
				pthread_mutex_lock(&gena_thread->mutex);
				if (gena_thread->stopped == 1) {
					pthread_join(gena_thread->thread, NULL);
				} else if (gena_thread->running == 1) {
					gena_thread->running = 0;
					pthread_cond_signal(&gena_thread->cond);
					while (gena_thread->stopped != 1) {
						pthread_cond_wait(&gena_thread->cond, &gena_thread->mutex);
					}
					pthread_join(gena_thread->thread, NULL);
				} else {
					debugf("this should not happen");
					assert(0);
				}
				list_del(&gena_thread->head);
				pthread_mutex_unlock(&gena_thread->mutex);
				pthread_mutex_destroy(&gena_thread->mutex);
				pthread_cond_destroy(&gena_thread->cond);
				close(gena_thread->fd);
				free(gena_thread);
			}
		}
		pthread_mutex_unlock(&gena->mutex);

		if (running == 0 || rc <= 0 || pfd.revents != POLLIN) {
			continue;
		}

		debugf("we have a new connection request");
		memset(&client, 0, sizeof(client));
		client_len = sizeof(client);
		fd = accept(pfd.fd, (struct sockaddr*) &client, &client_len);
		if (fd >= 0) {
			debugf("accepted new connection");
			gena_thread = (gena_thread_t *) malloc(sizeof(gena_thread_t));
			if (gena_thread == NULL) {
				debugf("malloc(sizeof(gena_thread_t) failed");
				close(fd);
				continue;
			}
			memset(gena_thread, 0, sizeof(gena_thread_t));
			gena_thread->fd = fd;
			pthread_mutex_init(&gena_thread->mutex, NULL);
			pthread_cond_init(&gena_thread->cond, NULL);
			pthread_mutex_lock(&gena_thread->mutex);
			pthread_create(&gena_thread->thread, NULL, gena_thread_loop, gena_thread);
			while (gena_thread->running != 1) {
				pthread_cond_wait(&gena_thread->cond, &gena_thread->mutex);
			}
			list_add(&gena_thread->head, &gena->threads);
			debugf("created and added new gena thread");
			pthread_mutex_unlock(&gena_thread->mutex);
		}
	}

	pthread_mutex_lock(&gena->mutex);
	debugf("stopped gena thread");
	gena->stopped = 1;
	pthread_cond_signal(&gena->cond);
	pthread_mutex_unlock(&gena->mutex);

	return NULL;
}

static int gena_init_server (gena_t *gena)
{
	int fd;
	socklen_t len;
	unsigned short port;
	struct sockaddr_in soc;

	port = gena->port;
	if (port < GENA_LISTEN_PORT) {
		port = GENA_LISTEN_PORT;
	}

	len = sizeof(soc);
	memset(&soc, 0, len);
        soc.sin_family = AF_INET;
	soc.sin_addr.s_addr = inet_addr(gena->address);
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

	if (listen(fd, GENA_LISTEN_MAX) < 0) {
		debugf("listen(%d) failed", GENA_LISTEN_MAX);
		close(fd);
		return -1;
	}

	if (getsockname(fd, (struct sockaddr *) &soc, &len) < 0) {
		close(fd);
		return -1;
	}
	gena->fd = fd;
	gena->port = ntohs(soc.sin_port);
	debugf("gena started on port: %s:%u", inet_ntoa(soc.sin_addr), gena->port);

	return 0;
}

unsigned short gena_getport (gena_t *gena)
{
	return gena->port;
}

const char * gena_getaddress (gena_t *gena)
{
	return gena->address;
}

gena_t * gena_init (char *address, unsigned short port)
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
	list_init(&gena->threads);
	gena_init_server(gena);

	pthread_mutex_init(&gena->mutex, NULL);
	pthread_cond_init(&gena->cond, NULL);
	pthread_create(&gena->thread, NULL, gena_loop, gena);

	pthread_mutex_lock(&gena->mutex);
	while (gena->running == 0) {
		pthread_cond_wait(&gena->cond, &gena->mutex);
	}
	debugf("started gena loop");
	pthread_mutex_unlock(&gena->mutex);
	debugf("initialized gena");
	return gena;
}

int gena_uninit (gena_t *gena)
{
	debugf("stopping gena thread");
	if (gena == NULL) {
		goto out;
	}
	pthread_mutex_lock(&gena->mutex);
	gena->running = 0;
	pthread_cond_signal(&gena->cond);
	while (gena->stopped != 1) {
		pthread_cond_wait(&gena->cond, &gena->mutex);
	}
	pthread_join(gena->thread, NULL);
	pthread_mutex_unlock(&gena->mutex);
	pthread_mutex_destroy(&gena->mutex);
	pthread_cond_destroy(&gena->cond);
	close(gena->fd);
	free(gena->address);
	free(gena);
out:	debugf("gena uninited");
	return 0;
}
