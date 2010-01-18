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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>

#include "platform.h"

struct socket_s {
	int type;
	int fd;
};

static inline int socket_type_bsd (socket_type_t type)
{
	switch (type) {
		case SOCKET_TYPE_DGRAM:  return SOCK_DGRAM;
		case SOCKET_TYPE_STREAM: return SOCK_STREAM;
	}
	return -1;
}

static inline int socket_event_bsd (socket_event_t event)
{
	int bsd;
	bsd = 0;
	if (event & SOCKET_EVENT_ERR)  bsd |= (POLLERR | POLLHUP | POLLNVAL);
	if (event & SOCKET_EVENT_IN)   bsd |= POLLIN;
	if (event & SOCKET_EVENT_OUT)  bsd |= POLLOUT;
	return bsd;
}

static inline socket_event_t socket_bsd_event (int bsd)
{
	socket_event_t event;
	event = 0;
	if (bsd & POLLERR)  event |= SOCKET_EVENT_ERR;
	if (bsd & POLLHUP)  event |= SOCKET_EVENT_ERR;
	if (bsd & POLLIN)   event |= SOCKET_EVENT_IN;
	if (bsd & POLLNVAL) event |= SOCKET_EVENT_ERR;
	if (bsd & POLLOUT)  event |= SOCKET_EVENT_OUT;
	return event;
}

socket_t * socket_open (socket_type_t type)
{
	socket_t *s;
	s = (socket_t *) malloc(sizeof(socket_t));
	if (s == NULL) {
		return NULL;
	}
	memset(s, 0, sizeof(socket_t));
	s->type = socket_type_bsd(type);
	s->fd = socket(AF_INET, s->type, 0);
	if (s->fd < 0) {
		free(s);
		return NULL;
	}
	return s;
}

int socket_bind (socket_t *socket, const char *address, int port)
{
	socklen_t len;
	struct sockaddr_in soc;
	len = sizeof(soc);
	memset(&soc, 0, len);
	soc.sin_family = AF_INET;
	soc.sin_addr.s_addr = inet_addr(address);
	soc.sin_port = htons(port);
	return bind(socket->fd, (struct sockaddr *) &soc, len);
}

int socket_listen (socket_t *socket, int backlog)
{
	return listen(socket->fd, backlog);
}

socket_t * socket_accept (socket_t *socket)
{
	socket_t *s;
	socklen_t client_len;
	struct sockaddr_in client;
	s = (socket_t *) malloc(sizeof(socket_t));
	if (s == NULL) {
		return NULL;
	}
	memset(s, 0, sizeof(socket_t));
	s->type = socket->type;
	memset(&client, 0, sizeof(struct sockaddr_in));
	client_len = sizeof(struct sockaddr_in);
	s->fd = accept(socket->fd, (struct sockaddr *) &client, &client_len);
	if (s->fd < 0) {
		free(s);
		return NULL;
	}
	return s;
}

int socket_connect (socket_t *socket, const char *address, int port, int timeout)
{
	long flags;
	struct pollfd pfd;
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(address);
	server.sin_port = htons(port);
	flags = fcntl(socket->fd, F_GETFL);
	if (flags < 0) {
		return -1;
	}
	flags |= O_NONBLOCK;
	if (fcntl(socket->fd, F_SETFL, flags) < 0) {
		return -1;
	}
	if (connect(socket->fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
		memset(&pfd, 0, sizeof(struct pollfd));
		pfd.fd = socket->fd;
		pfd.events = POLLOUT;
		if (poll(&pfd, 1, timeout) <= 0) {
			return -1;
		}
		if (pfd.revents != POLLOUT) {
			return -1;
		}
	}
	flags &= ~O_NONBLOCK;
	if (fcntl(socket->fd, F_SETFL, flags) < 0) {
		return -1;
	}
	return 0;
}

int socket_recv (socket_t *socket, void *buffer, int length)
{
	return recv(socket->fd, buffer, length, 0);
}

int socket_send (socket_t *socket, const void *buffer, int length)
{
	return send(socket->fd, buffer, length, 0);
}

int socket_recvfrom (socket_t *socket, void *buf, int length, char *address, int *port)
{
	int rc;
	socklen_t sender_length;
	struct sockaddr_in sender;
	sender_length = sizeof(struct sockaddr_in);
	rc = recvfrom(socket->fd, buf, length, 0, (struct sockaddr *) &sender, &sender_length);
	if (address) {
		inet_ntop(AF_INET, &(sender.sin_addr), address, SOCKET_IP_LENGTH);
	}
	if (port) {
		*port = ntohs(sender.sin_port);
	}
	return rc;
}

int socket_sendto (socket_t *socket, const void *buf, int length, const char *address, int port)
{
	socklen_t dest_length;
	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = inet_addr(address);
	dest.sin_port = htons(port);
	dest_length = sizeof(struct sockaddr_in);
	return sendto(socket->fd, buf, length, 0, (struct sockaddr *) &dest, dest_length);
}

int socket_poll (socket_t *socket, socket_event_t request, socket_event_t *result, int timeout)
{
	int rc;
	struct pollfd pfd;
	memset(&pfd, 0, sizeof(struct pollfd));
	pfd.fd = socket->fd;
	pfd.events = socket_event_bsd(request);
	rc = poll(&pfd, 1, timeout);
	if (result) {
		*result = socket_bsd_event(pfd.revents);
	}
	return rc;
}

int socket_close (socket_t *socket)
{
	if (socket) {
		close(socket->fd);
		free(socket);
	}
	return 0;
}

int socket_option_reuseaddr (socket_t *socket, int on)
{
	return setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
}

int socket_option_membership (socket_t *socket, const char *address, int on)
{
	struct hostent *h;
	struct ip_mreq mreq;
	struct in_addr mcastip;
	h = gethostbyname(address);
	if (h == NULL) {
		return -1;
	}
	memcpy(&mcastip, h->h_addr_list[0], h->h_length);
	mreq.imr_multiaddr.s_addr = mcastip.s_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (on) {
		return setsockopt(socket->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &mreq, sizeof(mreq));
	} else {
		return setsockopt(socket->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &mreq, sizeof(mreq));
	}
}

int socket_option_multicastttl (socket_t *socket, int ttl)
{
	return setsockopt(socket->fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &ttl, sizeof(ttl));
}

int socket_inet_aton (const char *address, unsigned int *baddress)
{
	int r;
	struct in_addr b;
	r = inet_aton(address, &b);
	if (r == 0) {
		return -1;
	}
	if (baddress != NULL) {
		*baddress = b.s_addr;
	}
	return 0;
}
