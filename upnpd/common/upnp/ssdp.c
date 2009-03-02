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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>

#include "upnp.h"

struct ssdp_s {
	int socket;
	int started;
	int stopped;
	int running;
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};

static const char *ssdp_ip = "239.255.255.250";
static const unsigned short ssdp_port = 1900;
static const unsigned int ssdp_buffer_length = 2500;
static const unsigned int ssdp_recv_timeout = 1000;

static void * ssdp_thread_loop (void *arg)
{
	int ret;
	int received;
	char *buffer;
	ssdp_t *ssdp;
	struct pollfd pfd;
	struct sockaddr_in sender;
	socklen_t sender_length;

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

int ssdp_advertise (ssdp_t *ssdp)
{
	pthread_mutex_lock(&ssdp->mutex);
	pthread_mutex_unlock(&ssdp->mutex);
	return 0;
}

ssdp_t * ssdp_init (const char *description, const unsigned int length)
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

	return ssdp;
}

int ssdp_uninit (ssdp_t *ssdp)
{
	pthread_mutex_lock(&ssdp->mutex);
	ssdp->running = 0;
	pthread_cond_signal(&ssdp->cond);
	while (ssdp->stopped == 0) {
		pthread_cond_wait(&ssdp->cond, &ssdp->mutex);
	}
	pthread_mutex_unlock(&ssdp->mutex);
	pthread_mutex_destroy(&ssdp->mutex);
	pthread_cond_destroy(&ssdp->cond);
	pthread_join(ssdp->thread, NULL);

	close(ssdp->socket);
	free(ssdp);

	return 0;
}
