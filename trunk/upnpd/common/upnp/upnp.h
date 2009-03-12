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

#ifndef UPNP_H_
#define UPNP_H_

typedef struct ssdp_s ssdp_t;
typedef struct gena_s gena_t;
typedef struct upnp_s upnp_t;

int ssdp_advertise (ssdp_t *ssdp, char *description);
ssdp_t * ssdp_init (void);
int ssdp_uninit (ssdp_t *ssdp);

unsigned short gena_getport (gena_t *gena);
const char * gena_getaddress (gena_t *gena);
gena_t * gena_init (char *address, unsigned short port);
int gena_uninit (gena_t *gena);

int upnp_advertise (upnp_t *upnp);
int upnp_register_device (upnp_t *upnp, const char *description);
char * upnp_getaddress (upnp_t *upnp);
unsigned short upnp_getport (upnp_t *upnp);
upnp_t * upnp_init (const char *host, const unsigned short port);
int upnp_uninit (upnp_t *upnp);

#endif /* UPNP_H_ */
