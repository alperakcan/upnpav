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

#include "metadata.h"

int main (int argc, char *argv[])
{
	int s = 0;
	metadata_snapshot_t *snapshot;

	while (1) {
		snapshot = metadata_snapshot_init(argv[1], 300, 150);
		if (snapshot == NULL) {
			break;
		}
		if (metadata_snapshot_obtain(snapshot, s) < 0) {
			metadata_snapshot_uninit(snapshot);
			break;
		}
		metadata_snapshot_uninit(snapshot);
		s += 60;
	}

	return 0;
}
