/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 - 2010 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 - 2010 CoreCodec, Inc., http://www.CoreCodec.com
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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <inttypes.h>

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

char * upnpd_interface_getaddr (const char *ifname)
{
	int sock;
	struct ifreq *ifr;
	struct ifreq ifrr;
	struct sockaddr_in sa;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return NULL;
	}

	ifr = &ifrr;
	ifrr.ifr_addr.sa_family = AF_INET;
	strncpy(ifrr.ifr_name, ifname, sizeof(ifrr.ifr_name));

	if (ioctl(sock, SIOCGIFADDR, ifr) < 0) {
		return NULL;
	}

	close(sock);
	return __inet_ntoa__(INADDR(ifr_addr.sa_data));
}

char * upnpd_interface_getmask (const char *ifname)
{
	int sock;
	struct ifreq *ifr;
	struct ifreq ifrr;
	struct sockaddr_in sa;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return NULL;
	}

	ifr = &ifrr;
	ifrr.ifr_addr.sa_family = AF_INET;
	strncpy(ifrr.ifr_name, ifname, sizeof(ifrr.ifr_name));

	if (ioctl(sock, SIOCGIFNETMASK, ifr) < 0) {
		return NULL;
	}

	close(sock);
	return __inet_ntoa__(INADDR(ifr_addr.sa_data));
}

int upnpd_interface_printall (void)
{
	int sockfd;
	int size  = 1;
	struct ifreq *ifr;
	struct ifconf ifc;
	struct sockaddr_in sa;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		return -1;
	}

	ifc.ifc_len = IFRSIZE;
	ifc.ifc_req = NULL;

	do {
		++size;
		if ((ifc.ifc_req = realloc(ifc.ifc_req, IFRSIZE)) == NULL) {
			return -1;
		}
		ifc.ifc_len = IFRSIZE;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
			return -1;
		}
	} while  (IFRSIZE <= ifc.ifc_len);

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
