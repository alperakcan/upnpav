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

typedef struct metadata_snapshot_s metadata_snapshot_t;

metadata_snapshot_t * metadata_snapshot_init (const char *path, int width, int height);
int metadata_snapshot_uninit (metadata_snapshot_t *snapshot);
int metadata_snapshot_obtain (metadata_snapshot_t *snapshot, unsigned int seconds);
