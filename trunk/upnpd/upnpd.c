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
#include <string.h>

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "upnpd.h"
#include "common.h"

#ifdef ENABLE_PLATFORM_COREC
#define DISABLE_DEMONIZE
#endif

#if defined(ENABLE_CONTROLLER)
extern upnpd_application_t controller;
#endif
#if defined(ENABLE_MEDIASERVER)
extern upnpd_application_t mediaserver;
#endif
#if defined(ENABLE_MEDIARENDERER)
extern upnpd_application_t mediarenderer;
#endif

static upnpd_application_t *upnpd_applications[] = {
#if defined(ENABLE_CONTROLLER)
	&controller,
#endif
#if defined(ENABLE_MEDIASERVER)
	&mediaserver,
#endif
#if defined(ENABLE_MEDIARENDERER)
	&mediarenderer,
#endif
	NULL,
};
#define NUM_ARRAY(array) (sizeof(array) / sizeof(*array))

static const struct option upnpd_options[] = {
	{"help", 0, 0, 'h'},
	{"device", 1, 0, 'd'},
	{"interface", 1, 0, 'i'},
	{"options", 1, 0, 'o'},
	{"verbose", 0, 0, 'v'},
#ifndef DISABLE_DEMONIZE
	{"background", 0, 0, 'b'},
#endif
	{0, 0, 0, 0},
};

int upnpd_help (char *pname)
{
	upnpd_application_t **a;
	printf("\n"
	"%s [options] --interface <name> --device <name> [--options <options>]\n"
	"  -h, --help              show this message\n"
	"  -d, --device <name>     use upnpd application named <name>\n"
	"  -i, --interface <name>  use network interface named <name>\n"
	"                          give name as 'list' to see the interfaces\n"
	"  -o, --options <options> device options comma separated\n"
#ifndef DISABLE_DEMONIZE
	"  -b, --background        daemonize\n"
#endif
	"  -v, --verbose           noisy debug\n",
	pname);
	printf("\n"
	"  %u devices compiled in\n",
	(unsigned int) NUM_ARRAY(upnpd_applications) - 1);
	for (a = upnpd_applications; *a; a++) {
		printf("    %-15s - %s\n", (*a)->name, (*a)->description);
	}
	printf("\n");
	return 0;
}

#ifndef DISABLE_DEMONIZE
static void signal_handler (int sig)
{
	switch (sig) {
		case SIGHUP:
			break;
		case SIGKILL:
			break;
	}
}
#endif

int main (int argc, char *argv[])
{
	int ret;
	int opt;
#ifndef DISABLE_DEMONIZE
	pid_t pid;
#endif
	int daemonize;
	int opt_index;
	char *device;
	char *options;
	char *interface;
	char *ipaddress;
	char *ifnetmask;
	upnpd_application_t **a;

	char *device_options;

	opt = 0;
	daemonize = 0;
	opt_index = 0;
	device = NULL;
	options = NULL;
	interface = NULL;
	device_options = NULL;

	while ((opt = getopt_long(argc, argv, "bvhd:i:o:", upnpd_options, &opt_index)) != -1) {
		switch (opt) {
			default:
			case 'h':
				upnpd_help(argv[0]);
				return 0;
			case 'd':
				device = strdup(optarg);
				if (device == NULL) {
					debugf(_DBG, "strdup('%s') failed", optarg);
					return -1;
				}
				break;
			case 'i':
				interface = optarg;
				break;
			case 'o':
				options = optarg;
				break;
			case 'v':
				platform_debug = 1;
				break;
#ifndef DISABLE_DEMONIZE
			case 'b':
				daemonize = 1;
#endif
				break;
		}
	}

	if (platform_init() < 0) {
		debugf(_DBG, "platform init failed");
		return -1;
	}
	if (interface != NULL && strcmp(interface, "list") == 0) {
		upnpd_interface_printall();
	}
	if (device == NULL || interface == NULL) {
		upnpd_help(argv[0]);
		platform_uninit();
		return -2;
	}
	ipaddress = upnpd_interface_getaddr(interface);
	if (ipaddress == NULL) {
		debugf(_DBG, "could not find interface %s", interface);
		free(device);
		platform_uninit();
		return -3;
	}
	ifnetmask = upnpd_interface_getmask(interface);
	if (ifnetmask == NULL) {
		debugf(_DBG, "could not find interface netmask %s", interface);
		free(device);
		platform_uninit();
		return -3;
	}

	if (asprintf(&device_options, "daemonize=%d,interface=%s,ipaddr=%s,netmask=%s%s%s", daemonize, interface, ipaddress, ifnetmask, (options) ? "," : "", (options) ? options : "") < 0) {
		debugf(_DBG, "asprintf failed for device_options");
		free(device);
		free(ipaddress);
		platform_uninit();
		return -4;
	}

#ifndef DISABLE_DEMONIZE
	if (daemonize == 1) {
		pid = fork();
		if (pid < 0) {
			debugf(_DBG, "fork(); failed");
			exit(1);
		}
		if (pid > 0) {
			exit(0);
		}
		setsid();
		chdir("/");
		signal(SIGCHLD,SIG_IGN); /* ignore child */
		signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
		signal(SIGTTOU,SIG_IGN);
		signal(SIGTTIN,SIG_IGN);
		signal(SIGHUP,signal_handler); /* catch hangup signal */
		signal(SIGTERM,signal_handler); /* catch kill signal */
	}
#endif

	debugf(_DBG, "starting with options: '%s'", device_options);

	ret = -5;
	for (a = upnpd_applications; *a; a++) {
		if (strcmp((*a)->name, device) == 0) {
			debugf(_DBG, "starting %s device", device);
			ret = (*a)->main(device_options);
			goto out;
		}
	}
	debugf(_DBG, "could not find device %s", device);
out:
	free(device);
	free(ifnetmask);
	free(ipaddress);
	free(device_options);
	platform_uninit();
	return ret;
}
