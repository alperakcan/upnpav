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

#include <stdlib.h>
#include <unistd.h>

#include "upnp.h"

int main (int argc, char *argv[])
{
	ssdp_t *ssdp;
	ssdp = ssdp_init();
	if (ssdp == NULL) {
		return -1;
	}

	while (1)
		sleep(1);

	ssdp_uninit(ssdp);
	return 0;
}
