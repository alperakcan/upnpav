/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 CoreCodec, Inc., http://www.CoreCodec.com
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

typedef struct database_s database_t;
typedef struct database_entry_s database_entry_t;

struct database_entry_s {
	char *id;
	char *class;
	char *parent;
	char *path;
	char *title;
	unsigned long long size;
	char *date;
	char *mime;
	char *dlna;
	unsigned long long childs;
	database_entry_t *next;
};

database_t * database_init (int delete);

int database_index (database_t *database);

int database_entry_free (database_entry_t *entry);

database_entry_t * database_query_entry (database_t *database, const char *entryid);

database_entry_t * database_query_parent (database_t *database, const char *parentid, unsigned long long start, unsigned long long count, unsigned long long *total);

database_entry_t * database_query_search (database_t *database, const char *parentid, unsigned long long start, unsigned long long count, unsigned long long *total, const char *searchflag);

unsigned long long database_insert (database_t *database,
		const char *class,
		const char *parentid,
		const char *path,
		const char *title,
		const unsigned long long size,
		const char *date,
		const char *mime,
		const char *dlna);

int database_uninit (database_t *database, int delete);
