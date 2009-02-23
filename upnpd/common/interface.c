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
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "common.h"

#define INADDR(x) (*(struct in_addr *) &ifr->x[sizeof sa.sin_port])
#define IFRSIZE   ((int) (size * sizeof (struct ifreq)))

static char * __inet_ntoa__ (struct in_addr in)
{
	char *buf = NULL;
	unsigned int saddr;
	buf = malloc(sizeof(char) * 16);
	if (buf == NULL) {
		return NULL;
	}
	saddr = ntohl(in.s_addr);
	sprintf(buf, "%d.%d.%d.%d", (saddr >> 0x18) & 0xff, (saddr >> 0x10) & 0xff, (saddr >> 0x08) & 0xff, (saddr >> 0x00) & 0xff);
	return buf;
}

char * interface_getaddr (char *ifname)
{
	int sock;
	struct ifreq *ifr;
	struct ifreq ifrr;
	struct sockaddr_in sa;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		debugf("cannot open socket");
		return NULL;
	}

	ifr = &ifrr;
	ifrr.ifr_addr.sa_family = AF_INET;
	strncpy(ifrr.ifr_name, ifname, sizeof(ifrr.ifr_name));

	if (ioctl(sock, SIOCGIFADDR, ifr) < 0) {
		debugf("no %s interface.", ifname);
		return NULL;
	}

	close(sock);
	return __inet_ntoa__(INADDR(ifr_addr.sa_data));
}

int interface_printall (void)
{
	int sockfd;
	int size  = 1;
	unsigned char *u;
	struct ifreq *ifr;
	struct ifconf ifc;
	struct sockaddr_in sa;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		debugf("cannot open socket");
		return -1;
	}

	ifc.ifc_len = IFRSIZE;
	ifc.ifc_req = NULL;

	do {
		++size;
		if ((ifc.ifc_req = realloc(ifc.ifc_req, IFRSIZE)) == NULL) {
			debugf("out of memory");
			return -1;
		}
		ifc.ifc_len = IFRSIZE;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
			debugf("ioctl SIOCFIFCONF");
			return -1;
		}
	} while  (IFRSIZE <= ifc.ifc_len);

#if 0
	{
		struct sockaddr ifa;
		autodetect_getaddr(sockfd, "eth0", &ifa);
	}
#endif

	printf("interfaces;\n");
	ifr = ifc.ifc_req;
	for (; (char *) ifr < (char *) ifc.ifc_req + ifc.ifc_len; ++ifr) {
		if (ifr->ifr_addr.sa_data == (ifr + 1)->ifr_addr.sa_data) {
			continue;  /* duplicate, skip it */
		}
		if (ioctl(sockfd, SIOCGIFFLAGS, ifr)) {
			continue;  /* failed to get flags, skip it */
		}

		printf("\n");
		printf("interface : %s\n", ifr->ifr_name);
		printf("ip address: %s\n", inet_ntoa(INADDR(ifr_addr.sa_data)));

		if (ioctl(sockfd, SIOCGIFHWADDR, ifr) == 0) {
			 switch (ifr->ifr_hwaddr.sa_family) {
				default:
					continue;
				case  ARPHRD_NETROM:
				case  ARPHRD_ETHER:
				case  ARPHRD_PPP:
				case  ARPHRD_EETHER:
				case  ARPHRD_IEEE802:
				break;
			}
			u = (unsigned char *) &ifr->ifr_addr.sa_data;
			if (u[0] + u[1] + u[2] + u[3] + u[4] + u[5]) {
				printf("hw address: %02x.%02x.%02x.%02x.%02x.%02x\n",
					u[0] & 0xff, u[1] & 0xff, u[2] & 0xff,
					u[3] & 0xff, u[4] & 0xff, u[5] & 0xff);
			}
		}


		if ((ioctl(sockfd, SIOCGIFNETMASK, ifr) == 0) &&
		    strcmp("255.255.255.255", inet_ntoa(INADDR(ifr_addr.sa_data)))) {
			printf("netmask   : %s\n", inet_ntoa(INADDR(ifr_addr.sa_data)));
		}

		if (ifr->ifr_flags & IFF_BROADCAST) {
			if ((ioctl(sockfd, SIOCGIFBRDADDR, ifr) == 0) &&
			    strcmp("0.0.0.0", inet_ntoa(INADDR(ifr_addr.sa_data)))) {
				printf("broadcast : %s\n", inet_ntoa(INADDR(ifr_addr.sa_data)));
			}
		}
	}
	printf("\n");

	free(ifc.ifc_req);
	close(sockfd);

	return 0;
}
