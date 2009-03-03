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

struct upnp_s {
	char *host;
	unsigned short port;
	ssdp_t *ssdp;
};

int upnp_advertise (upnp_t *upnp)
{
	return ssdp_advertise(upnp->ssdp);
}

int upnp_register_device (upnp_t *upnp, const char *description, const unsigned int length)
{
	upnp->ssdp = ssdp_init(description, length);
	if (upnp->ssdp == NULL) {
		return -1;
	}
	return 0;
}

char * upnp_server_address (upnp_t *upnp)
{
	return upnp->host;
}

unsigned short upnp_server_port (upnp_t *upnp)
{
	return upnp->port;
}

upnp_t * upnp_init (const char *host, const unsigned short port)
{
	upnp_t *upnp;
	upnp = (upnp_t *) malloc(sizeof(upnp_t));
	if (upnp == NULL) {
		return NULL;
	}
	memset(upnp, 0, sizeof(upnp_t));
	upnp->host = strdup(host);
	if (upnp->host == NULL) {
		free(upnp);
		return NULL;
	}
	upnp->port = port;
	return upnp;
}

int upnp_uninit (upnp_t *upnp)
{
	if (upnp->ssdp != NULL) {
		ssdp_uninit(upnp->ssdp);
	}
	free(upnp->host);
	free(upnp);
	return 0;
}
