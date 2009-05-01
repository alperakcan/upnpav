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
#include <stdarg.h>
#include <pthread.h>

unsigned char upnpd_debug = 1;

void debug_debugf (char *file, int line, const char *func, char *fmt, ...)
{
	int n;
	int s;
	char *p;
	va_list args;

	if (upnpd_debug == 0) {
		return;
	}

	s = 100;
	if ((p = malloc(sizeof(char) * s)) == NULL) {
		fprintf(stderr, "exiting [%s (%s:%d)]\n", __FUNCTION__, __FILE__, __LINE__);
		goto err;
	}

	while (1) {
		va_start(args, fmt);
		n = vsnprintf(p, s, fmt, args);
		va_end(args);
		if (n > -1 && n < s) {
			break;
		}
		if (n > -1) {
			s = n + 1;
		} else {
			s *= 2;
		}
		if ((p = realloc(p, s))  == NULL) {
			goto err;
		}
	}

	fprintf(stderr, "\033[1;30m[0x%08X] \033[0m%s \033[1;30m[%s (%s:%d)]\033[0m\n", (unsigned int) pthread_self(), p, func, file, line);
	fflush(stderr);
	free(p);

	return;
err:	exit(1);
}
