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

#ifndef UPNPD_H_
#define UPNPD_H_

/** @defgroup deamon Deamon
  * @brief detailed description
  */

/** @addtogroup deamon */
/*@{*/

/** upnpd application struct
  */
typedef struct upnpd_application_s {
	/** application name */
	char *name;
	/** application description */
	char *description;
	/** application main function */
	int (*main) (char *options);
} upnpd_application_t;

/* upnpd.c */

/** @brief printf help text for upnpd application
  *
  * @param *pname - program name
  * @returns 0 on success
  */
int upnpd_help (char *pname);

/*@}*/

#endif /*UPNPD_H_*/
