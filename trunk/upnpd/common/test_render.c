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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "common.h"

extern unsigned char upnpd_debug;

int main (int argc, char *argv[])
{
	render_t *render;
	upnpd_debug = 1;
	render = render_init("ffmpeg", NULL);
	if (render == NULL) {
		debugf("render_init() failed");
	}
	sleep(1);
	if (render_play(render, "/home/mp3/mp3/Alanis Morisette/01-AudioTrack 01.mp31")) {
		debugf("render_play() failed");
	}
	sleep(10);
	if (render_uninit(render)) {
		debugf("render_uninit() failed");
	}
	return 0;
}
