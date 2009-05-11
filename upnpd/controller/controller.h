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

typedef struct controller_gui_s controller_gui_t;
typedef struct image_s image_t;

struct image_s {
	unsigned int width;
	unsigned int height;
	unsigned int *data;
};

struct controller_gui_s {
	char *name;
	int (*main) (char *options);
};
