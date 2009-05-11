/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 - 2009 by Alper Akcan
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
#include <getopt.h>
#include <signal.h>
#include <inttypes.h>
#include <pthread.h>

#include "gena.h"
#include "upnp.h"
#include "upnpd.h"
#include "common.h"

extern unsigned char upnp_debug;

#if defined(ENABLE_CONTROLLER)
extern upnpd_application_t controller;
#endif
#if defined(ENABLE_MEDIASERVER)
extern upnpd_application_t mediaserver;
#endif

static upnpd_application_t *upnpd_applications[] = {
#if defined(ENABLE_CONTROLLER)
	&controller,
#endif
#if defined(ENABLE_MEDIASERVER)
	&mediaserver,
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
	{"background", 0, 0, 'b'},
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
	"  -b, --background        daemonize\n"
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

static void signal_handler (int sig)
{
	switch (sig) {
		case SIGHUP:
			break;
		case SIGKILL:
			break;
	}
}

int main (int argc, char *argv[])
{
	int ret;
	int opt;
	pid_t pid;
	int daemonize;
	int opt_index;
	char *device;
	char *options;
	char *interface;
	char *ipaddress;
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
					debugf("strdup('%s') failed", optarg);
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
				upnp_debug = 1;
				break;
			case 'b':
				daemonize = 1;
				break;
		}
	}

	if (interface != NULL && strcmp(interface, "list") == 0) {
		interface_printall();
	}
	if (device == NULL || interface == NULL) {
		upnpd_help(argv[0]);
		return -2;
	}
	ipaddress = interface_getaddr(interface);
	if (ipaddress == NULL) {
		debugf("could not find interface %s", interface);
		free(device);
		return -3;
	}

	if (asprintf(&device_options, "daemonize=%d,interface=%s%s%s", daemonize, ipaddress, (options) ? "," : "", (options) ? options : "") < 0) {
		debugf("asprintf failed for device_options");
		free(device);
		free(ipaddress);
		return -4;
	}

	if (daemonize == 1) {
		pid = fork();
		if (pid < 0) {
			debugf("fork(); failed");
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

	debugf("starting with options: '%s'", device_options);

	ret = -5;
	for (a = upnpd_applications; *a; a++) {
		if (strcmp((*a)->name, device) == 0) {
			debugf("starting %s device", device);
			ret = (*a)->main(device_options);
			goto out;
		}
	}
	debugf("could not find device %s", device);
out:
	free(device);
	free(ipaddress);
	free(device_options);
	return ret;
}
