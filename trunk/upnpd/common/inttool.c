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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

char * uint32tostr (char *out, uint32_t val)
{
	sprintf(out, "%u", val);
	return out;
}

uint32_t strtouint32 (const char *in)
{
	uint32_t r;
	if (in != NULL) {
		if (strcmp(in, "true") == 0) {
			return 1;
		} else if (strcmp(in, "false") == 0) {
			return 0;
		} else if (sscanf(in, "%u", &r) == 1) {
			return r;
		}
	}
	return 0;
}

int32_t strtoint32 (const char *in)
{
	int32_t r;
	if (in != NULL) {
		if (strcmp(in, "true") == 0) {
			return 1;
		} else if (strcmp(in, "false") == 0) {
			return 0;
		} else if (sscanf(in, "%d", &r) == 1) {
			return r;
		}
	}
	return 0;
}
