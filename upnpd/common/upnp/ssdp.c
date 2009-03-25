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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>

#include "upnp.h"
#include "list.h"

struct ssdp_s {
	int socket;
	int started;
	int stopped;
	int running;
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	list_t devices;
};

typedef enum {
	SSDP_TYPE_UNKNOWN,
	SSDP_TYPE_NOTIFY,
	SSDP_TYPE_MSEARCH,
} ssdp_type_t;

typedef struct ssdp_request_notify_s {
	ssdp_type_t request;
	char *host;
	char *nt;
	char *nts;
	char *usn;
	char *al;
	char *location;
	char *server;
	char *cachecontrol;
} ssdp_request_notify_t;

typedef struct ssdp_request_search_s {
	ssdp_type_t request;
	char *s;
	char *host;
	char *man;
	char *st;
	char *mx;
} ssdp_request_search_t;

typedef union ssdp_request_u {
	ssdp_type_t request;
	ssdp_request_notify_t notify;
	ssdp_request_search_t search;
} ssdp_request_t;

struct ssdp_device_s {
	list_t head;
	char *nt;
	char *usn;
	char *location;
	char *server;
	int age;
};

static const char *ssdp_ip = "239.255.255.250";
static const unsigned short ssdp_port = 1900;
static const unsigned int ssdp_buffer_length = 2500;
static const unsigned int ssdp_recv_timeout = 1000;
static const unsigned int ssdp_ttl = 4;
static const unsigned int ssdp_pause = 100;

static int ssdp_advertise_send (const char *buffer, struct sockaddr_in *dest);
static char * ssdp_advertise_buffer (ssdp_device_t *device, int answer);

static char * ssdp_trim (char *buffer)
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

static ssdp_device_t * ssdp_device_init (char *nt, char *usn, char *location, char *server, int age)
{
	ssdp_device_t *d;
	d = (ssdp_device_t *) malloc(sizeof(ssdp_device_t));
	if (d == NULL) {
		return NULL;
	}
	memset(d, 0, sizeof(ssdp_device_t));
	d->nt = strdup(nt);
	d->usn = strdup(usn);
	d->location = strdup(location);
	d->server = strdup(server);
	d->age = (age < ssdp_recv_timeout) ? ssdp_recv_timeout : age;
	if (d->nt == NULL ||
	    d->usn == NULL ||
	    d->location == NULL ||
	    d->server == NULL) {
		free(d->nt);
		free(d->usn);
		free(d->location);
		free(d->server);
		free(d);
		return NULL;
	}
	return d;
}

static int ssdp_device_uninit (ssdp_device_t *device)
{
	free(device->nt);
	free(device->usn);
	free(device->location);
	free(device->server);
	free(device);
	return 0;
}

static ssdp_request_t * ssdp_request_init (void)
{
	ssdp_request_t *r;
	r = (ssdp_request_t *) malloc(sizeof(ssdp_request_t));
	if (r == NULL) {
		return NULL;
	}
	memset(r, 0, sizeof(ssdp_request_t));
	r->request = SSDP_TYPE_UNKNOWN;
	return r;
}

static int ssdp_request_uninit_notify (ssdp_request_notify_t *n)
{
	free(n->al);
	free(n->location);
	free(n->server);
	free(n->cachecontrol);
	free(n->host);
	free(n->nt);
	free(n->nts);
	free(n->usn);
	return 0;
}

static int ssdp_request_uninit (ssdp_request_t *r)
{
	switch (r->request) {
		case SSDP_TYPE_MSEARCH:
			free(r->search.host);
			free(r->search.man);
			free(r->search.mx);
			free(r->search.s);
			free(r->search.st);
			break;
		case SSDP_TYPE_NOTIFY:
			ssdp_request_uninit_notify(&r->notify);
			break;
		case SSDP_TYPE_UNKNOWN:
			break;
	}
	free(r);
	return 0;
}

static int ssdp_request_valid (ssdp_request_t *request)
{
	switch (request->request) {
		case SSDP_TYPE_MSEARCH:
			if (request->search.st == NULL ||
			    request->search.man == NULL) {
				return -1;
			}
			if (strcasecmp(request->search.man, "ssdp:discover") == 0) {
				return 0;
			}
			break;
		case SSDP_TYPE_NOTIFY:
			if (request->notify.nt == NULL ||
			    request->notify.nts == NULL ||
			    request->notify.usn == NULL ||
			    (request->notify.al == NULL && request->notify.location == NULL)) {
				return -1;
			}
			if (strcasecmp(request->notify.nts, "ssdp:alive") == 0) {
				return 0;
			} else if (strcasecmp(request->notify.nts, "ssdp:byebye") == 0) {
				return 0;
			}
			break;
		default:
			break;
	}
	return -1;
}

static ssdp_request_t * ssdp_parse (char *buffer, int length)
{
	char *ptr;
	char *line;
	ssdp_request_t *request;
	request = ssdp_request_init();
	if (request == NULL) {
		return NULL;
	}
	ptr = buffer;
	while (ptr < buffer + length) {
		line = ptr;
		while (ptr < buffer + length) {
			if (*ptr == '\r') {
				*ptr = '\0';
			} else if (*ptr == '\n') {
				*ptr++ = '\0';
				break;
			}
			ptr++;
		}
		if (strncasecmp(line, "M-SEARCH", 8) == 0) {
			if (request->request != SSDP_TYPE_UNKNOWN) {
				free(request);
				return NULL;
			}
			request->request = SSDP_TYPE_MSEARCH;
		} else if (strncasecmp(line, "NOTIFY", 6) == 0) {
			if (request->request != SSDP_TYPE_UNKNOWN) {
				free(request);
				return NULL;
			}
			request->request = SSDP_TYPE_NOTIFY;
		}
		if (request->request == SSDP_TYPE_NOTIFY) {
			if (strncasecmp(line, "Host:", 5) == 0) {
				request->notify.host = strdup(ssdp_trim(line + 5));
			} else if (strncasecmp(line, "NT:", 3) == 0) {
				request->notify.nt = strdup(ssdp_trim(line + 3));
			} else if (strncasecmp(line, "NTS:", 4) == 0) {
				request->notify.nts = strdup(ssdp_trim(line + 4));
			} else if (strncasecmp(line, "USN:", 4) == 0) {
				request->notify.usn = strdup(ssdp_trim(line + 4));
			} else if (strncasecmp(line, "AL:", 3) == 0) {
				request->notify.al = strdup(ssdp_trim(line + 3));
			} else if (strncasecmp(line, "LOCATION:", 9) == 0) {
				request->notify.location = strdup(ssdp_trim(line + 9));
			} else if (strncasecmp(line, "Cache-Control:", 14) == 0) {
				request->notify.cachecontrol = strdup(ssdp_trim(line + 14));
			} else if (strncasecmp(line, "Server:", 7) == 0) {
				request->notify.server = strdup(ssdp_trim(line + 7));
			}
		} else if (request->request == SSDP_TYPE_MSEARCH) {
			if (strncasecmp(line, "S:", 2) == 0) {
				request->search.s = strdup(ssdp_trim(line + 2));
			} else if (strncasecmp(line, "Host:", 5) == 0) {
				request->search.host = strdup(ssdp_trim(line + 5));
			} else if (strncasecmp(line, "Man:", 4) == 0) {
				request->search.man = strdup(ssdp_trim(line + 4));
			} else if (strncasecmp(line, "ST:", 3) == 0) {
				request->search.st = strdup(ssdp_trim(line + 3));
			} else if (strncasecmp(line, "MX:", 3) == 0) {
				request->search.mx = strdup(ssdp_trim(line + 3));
			}
		}
	}
	if (ssdp_request_valid(request) != 0) {
		ssdp_request_uninit(request);
		request = NULL;
	}
	return request;
}

static int ssdp_request_handler (ssdp_t *ssdp, ssdp_request_t *request, struct sockaddr_in *sender)
{
	char *buffer;
	ssdp_device_t *d;
	pthread_mutex_lock(&ssdp->mutex);
	switch (request->request) {
		case SSDP_TYPE_MSEARCH:
			list_for_each_entry(d, &ssdp->devices, head) {
				if (strcasecmp(d->nt, request->search.st) == 0) {
					buffer = ssdp_advertise_buffer(d, 1);
					if (buffer != NULL) {
						ssdp_advertise_send(buffer, sender);
						free(buffer);
					}
				}
			}
			break;
		case SSDP_TYPE_NOTIFY:
			break;
		case SSDP_TYPE_UNKNOWN:
		default:
			break;
	}
	pthread_mutex_unlock(&ssdp->mutex);
	return 0;
}

static void * ssdp_thread_loop (void *arg)
{
	int ret;
	int received;
	char *buffer;
	ssdp_t *ssdp;
	struct pollfd pfd;
	socklen_t sender_length;
	ssdp_request_t *request;
	struct sockaddr_in sender;

	ssdp = (ssdp_t *) arg;

	pthread_mutex_lock(&ssdp->mutex);
	ssdp->started = 1;
	ssdp->running = 1;
	pthread_cond_signal(&ssdp->cond);
	pthread_mutex_unlock(&ssdp->mutex);

	buffer = (char *) malloc(sizeof(char) * (ssdp_buffer_length + 1));
	if (buffer == NULL) {
		goto out;
	}

	while (1) {
		pthread_mutex_lock(&ssdp->mutex);
		if (ssdp->running == 0 || ssdp->socket < 0) {
			pthread_mutex_unlock(&ssdp->mutex);
			break;
		}
		memset(&pfd, 0, sizeof(struct pollfd));
		pfd.fd = ssdp->socket;
		pfd.events = POLLIN;
		pfd.revents = 0;
		pthread_mutex_unlock(&ssdp->mutex);
		ret = poll(&pfd, 1, ssdp_recv_timeout);
		if (!ret) {
			continue;
		} else {
			if (pfd.revents != POLLIN) {
				break;
			}
		}
		sender_length = sizeof(struct sockaddr_in);
		received = recvfrom(pfd.fd, buffer, ssdp_buffer_length, 0, (struct sockaddr *) &sender, &sender_length);
		if (received <= 0) {
			continue;
		}
		buffer[received] = '\0';
		request = ssdp_parse(buffer, received);
		if (request != NULL) {
			ssdp_request_handler(ssdp, request, &sender);
			ssdp_request_uninit(request);
		}
	}
out:
	pthread_mutex_lock(&ssdp->mutex);
	ssdp->stopped = 1;
	ssdp->running = 0;
	if (buffer != NULL) {
		free(buffer);
	}
	pthread_cond_signal(&ssdp->cond);
	pthread_mutex_unlock(&ssdp->mutex);

	return NULL;
}

static int ssdp_init_server (ssdp_t *ssdp)
{
	int on;
	struct hostent *h;
	struct ip_mreq mreq;
	struct in_addr mcastip;
	struct sockaddr_in addr;

	h = gethostbyname(ssdp_ip);
	if (h == NULL) {
		return -1;
	}

	ssdp->socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (ssdp->socket < 0) {
		return -2;
	}

	on = 1;
	if (setsockopt(ssdp->socket, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0) {
		close(ssdp->socket);
		return -1;
	}

	addr.sin_family = AF_INET;
	inet_aton(ssdp_ip, &addr.sin_addr);
	addr.sin_port = htons(ssdp_port);
	if (bind(ssdp->socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(ssdp->socket);
		return -1;
	}

	memcpy(&mcastip, h->h_addr_list[0], h->h_length);
	mreq.imr_multiaddr.s_addr = mcastip.s_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	setsockopt(ssdp->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &mreq, sizeof(mreq));

	pthread_mutex_init(&ssdp->mutex, NULL);
	pthread_cond_init(&ssdp->cond, NULL);
	pthread_mutex_lock(&ssdp->mutex);
	pthread_create(&ssdp->thread, NULL, ssdp_thread_loop, ssdp);
	while (ssdp->started == 0) {
		pthread_cond_wait(&ssdp->cond, &ssdp->mutex);
	}
	pthread_mutex_unlock(&ssdp->mutex);

	return 0;
}

static int ssdp_advertise_send (const char *buffer, struct sockaddr_in *dest)
{
	int c;
	int rc;
	int ttl;
	int sock;
	int socklen;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		return -1;
	}
	ttl = ssdp_ttl;
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &ttl, sizeof(int));
	socklen = sizeof(struct sockaddr_in);
	for (c = 0; c < 2; c++) {
		rc = sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *) dest, socklen);
		usleep(ssdp_pause * 1000);
	}
	close(sock);
	return 0;
}

static char * ssdp_advertise_buffer (ssdp_device_t *device, int answer)
{
	int ret;
	char *buffer;
	const char *format_advertise =
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: %s:%d\r\n"
		"CACHE-CONTROL: max-age=%d\r\n"
		"LOCATION: %s\r\n"
		"NT: %s\r\n"
		"NTS: ssdp:alive\r\n"
		"SERVER: %s\r\n"
		"USN: %s\r\n"
		"\r\n";
	const char *format_answer =
		"HTTP/1.1 200 OK\r\n"
		"EXT:\r\n"
		"CACHE-CONTROL: max-age=%d\r\n"
		"LOCATION: %s\r\n"
		"ST: %s\r\n"
		"SERVER: %s\r\n"
		"USN: %s\r\n"
		"CONTENT-LENGTH: 0\r\n"
		"\r\n";
	if (answer == 1) {
		ret = asprintf(
			&buffer,
			format_answer,
			device->age / 1000,
			device->location,
			device->nt,
			device->server,
			device->usn);
	} else {
		ret = asprintf(
			&buffer,
			format_advertise,
			ssdp_ip,
			ssdp_port,
			device->age / 1000,
			device->location,
			device->nt,
			device->server,
			device->usn);
	}
	if (ret < 0) {
		return NULL;
	}
	return buffer;
}

int ssdp_advertise (ssdp_t *ssdp)
{
	char *buffer;
	ssdp_device_t *d;
	struct sockaddr_in dest;
	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = inet_addr(ssdp_ip);
	dest.sin_port = htons(ssdp_port);
	pthread_mutex_lock(&ssdp->mutex);
	list_for_each_entry(d, &ssdp->devices, head) {
		buffer = ssdp_advertise_buffer(d, 0);
		ssdp_advertise_send(buffer, &dest);
		free(buffer);
	}
	pthread_mutex_unlock(&ssdp->mutex);
	return 0;
}

int ssdp_register (ssdp_t *ssdp, char *nt, char *usn, char *location, char *server, int age)
{
	ssdp_device_t *d;
	pthread_mutex_lock(&ssdp->mutex);
	printf("registering\n"
		"  nt      : %s\n"
		"  usn     : %s\n"
		"  location: %s\n"
		"  server  : %s\n"
		"  age     : %d\n",
		nt,
		usn,
		location,
		server,
		age);
	d = ssdp_device_init(nt, usn, location, server, age);
	if (d != NULL) {
		list_add(&d->head, &ssdp->devices);
	}
	pthread_mutex_unlock(&ssdp->mutex);
	return 0;
}

ssdp_t * ssdp_init (void)
{
	ssdp_t *ssdp;
	ssdp = (ssdp_t *) malloc(sizeof(ssdp_t));
	if (ssdp == NULL) {
		return NULL;
	}
	memset(ssdp, 0, sizeof(ssdp_t));
	if (ssdp_init_server(ssdp) != 0) {
		free(ssdp);
		return NULL;
	}
	list_init(&ssdp->devices);
	return ssdp;
}

int ssdp_uninit (ssdp_t *ssdp)
{
	ssdp_device_t *d, *dn;
	pthread_mutex_lock(&ssdp->mutex);
	ssdp->running = 0;
	pthread_cond_signal(&ssdp->cond);
	while (ssdp->stopped == 0) {
		pthread_cond_wait(&ssdp->cond, &ssdp->mutex);
	}
	pthread_join(ssdp->thread, NULL);
	list_for_each_entry_safe(d, dn, &ssdp->devices, head) {
		list_del(&d->head);
		ssdp_device_uninit(d);
	}
	pthread_mutex_unlock(&ssdp->mutex);
	pthread_mutex_destroy(&ssdp->mutex);
	pthread_cond_destroy(&ssdp->cond);
	close(ssdp->socket);
	free(ssdp);
	return 0;
}
