/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 - 2010 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2009 - 2010 CoreCodec, Inc., http://www.CoreCodec.com
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

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "platform.h"
#include "database.h"

#include "sqlite3.h"

typedef struct database_arg_s {
	sqlite3 *database;
	void *args;
} database_arg_t;

struct database_s {
	char *name;
	sqlite3 *database;
};

int upnpd_database_uninit (database_t *database, int delete)
{
	upnpd_sqlite3_close(database->database);
	if (delete == 1) {
		unlink(database->name);
	}
	free(database);
	return 0;
}

database_t * upnpd_database_init (int remove)
{
	database_t *db;

	db = (database_t *) malloc(sizeof(database_t));
	if (db == NULL) {
		return NULL;
	}
	memset(db, 0, sizeof(database_t));

	db->name = "/tmp/upnpd.sqlite3";

	if (remove == 1) {
		unlink(db->name);
	}

	upnpd_sqlite3_open(db->name, &db->database);
	upnpd_sqlite3_busy_timeout(db->database, 5000);

	if (remove) {
		upnpd_sqlite3_exec(db->database,
			"CREATE TABLE OBJECT ("
			"  KEY INTEGER PRIMARY KEY AUTOINCREMENT,"
			"  ID TEXT NOT NULL,"
			"  CLASS TEXT NOT NULL,"
			"  PARENT INTEGER NOT NULL,"
			"  DETAIL INTEGER NOT NULL);",
			0, 0, 0);

		upnpd_sqlite3_exec(db->database,
			"CREATE TABLE DETAIL ("
			"  ID INTEGER PRIMARY KEY AUTOINCREMENT,"
			"  PATH TEXT NOT NULL,"
			"  TITLE TEXT NOT NULL,"
			"  SIZE TEXT NOT NULL,"
			"  DURATION TEXT NOT NULL,"
			"  DATE TEXT NOT NULL,"
			"  MIME TEXT NOT NULL,"
			"  DLNA TEXT NOT NULL);",
			0, 0, 0);
	}

	return db;
}

int upnpd_database_index (database_t *database)
{
	upnpd_sqlite3_exec(database->database, "CREATE INDEX INDEX_OBJECT on DETAIL(ID);", 0, 0, 0);
	upnpd_sqlite3_exec(database->database, "CREATE INDEX INDEX_OBJECT on OBJECT(ID);", 0, 0, 0);
	upnpd_sqlite3_exec(database->database, "CREATE INDEX INDEX_OBJECT on OBJECT(CLASS);", 0, 0, 0);
	upnpd_sqlite3_exec(database->database, "CREATE INDEX INDEX_OBJECT on OBJECT(PARENT);", 0, 0, 0);
	upnpd_sqlite3_exec(database->database, "CREATE INDEX INDEX_OBJECT on OBJECT(ID, PARENT);", 0, 0, 0);
	upnpd_sqlite3_exec(database->database, "CREATE INDEX INDEX_OBJECT on OBJECT(ID, DETAIL);", 0, 0, 0);
	return 0;
}

static int database_count_callback (void *args, int argc, char **argv, char **azColName)
{
	unsigned long long *total;
	total = (unsigned long long *) args;
	if (argc < 1) {
		*total = 0;
	} else {
		*total = atoll(argv[0]);
	}
	return 0;
}

static int database_query_callback (void *args, int argc, char **argv, char **azColName)
{
	char *sql;
	database_arg_t *darg;
	database_entry_t *tmp;
	database_entry_t *prev;
	database_entry_t *entry;
	database_entry_t **root;

	darg = (database_arg_t *) args;
	root = (database_entry_t **) darg->args;

	entry = (database_entry_t *) malloc(sizeof(database_entry_t));
	if (entry == NULL) {
		return 0;
	}
	memset(entry, 0, sizeof(database_entry_t));

	entry->id = strdup(argv[0]);
	entry->class = strdup(argv[1]);
	entry->parent = strdup(argv[2]);
	entry->path = strdup(argv[3]);
	entry->title = strdup(argv[4]);
	entry->size = strtoull(argv[5], NULL, 10);
	entry->duration = strdup(argv[6]);
	entry->date = strdup(argv[7]);
	entry->mime = strdup(argv[8]);
	entry->dlna = strdup(argv[9]);

	if (strcmp(entry->class, "object.container.storageFolder") == 0) {
		sql = upnpd_sqlite3_mprintf(
				"SELECT count(*)"
				"  from OBJECT o"
				"  where o.PARENT = %s;",
				entry->id);
		upnpd_sqlite3_exec(darg->database, sql, database_count_callback, (void *) &entry->childs, 0);
		upnpd_sqlite3_free(sql);
	}

	if (*root == NULL) {
		*root = entry;
	} else {
		tmp = *root;
		prev = NULL;
		while (tmp != NULL) {
			prev = tmp;
			tmp = tmp->next;
		}
		prev->next = entry;
	}

	return 0;
}

database_entry_t * upnpd_database_query_entry (database_t *database, const char *entryid)
{
	char *sql;
	database_arg_t darg;
	database_entry_t *e;
	e = NULL;
	sql = upnpd_sqlite3_mprintf(
			"SELECT o.ID,"
			"       o.CLASS,"
			"       o.PARENT,"
			"       d.PATH,"
			"       d.TITLE,"
			"       d.SIZE,"
			"       d.DURATION,"
			"       d.DATE,"
			"       d.MIME,"
			"       d.DLNA"
			"  from OBJECT o left join DETAIL d on (d.ID = o.DETAIL)"
			"  where o.ID = '%s';",
			entryid);
	darg.database = database->database;
	darg.args = (void *) &e;
	upnpd_sqlite3_exec(database->database, sql, database_query_callback, (void *) &darg, 0);
	upnpd_sqlite3_free(sql);
	return e;
}

database_entry_t * upnpd_database_query_parent (database_t *database, const char *parentid, unsigned long long start, unsigned long long count, unsigned long long *total)
{
	char *sql;
	database_arg_t darg;
	database_entry_t *e;
	e = NULL;
	*total = 0;
	sql = upnpd_sqlite3_mprintf(
			"SELECT count(*)"
			"  from OBJECT o"
			"  where o.PARENT = '%s';",
			parentid);
	upnpd_sqlite3_exec(database->database, sql, database_count_callback, (void *) total, 0);
	upnpd_sqlite3_free(sql);
	if (*total == 0) {
		return NULL;
	}
	sql = upnpd_sqlite3_mprintf(
			"SELECT o.ID,"
			"       o.CLASS,"
			"       o.PARENT,"
			"       d.PATH,"
			"       d.TITLE,"
			"       d.SIZE,"
			"       d.DURATION,"
			"       d.DATE,"
			"       d.MIME,"
			"       d.DLNA"
			"  from OBJECT o left join DETAIL d on (d.ID = o.DETAIL)"
			"  where o.PARENT = '%s' order by d.TITLE limit %llu, %llu;",
			parentid,
			start,
			count);
	darg.database = database->database;
	darg.args = (void *) &e;
	upnpd_sqlite3_exec(database->database, sql, database_query_callback, (void *) &darg, 0);
	upnpd_sqlite3_free(sql);
	if (e == NULL) {
		*total = 0;
	}
	return e;
}

database_entry_t * upnpd_database_query_search (database_t *database, const char *parentid, unsigned long long start, unsigned long long count, unsigned long long *total, const char *searchflag)
{
	char *sql;
	char *nsearch;
	database_arg_t darg;
	database_entry_t *e;
	e = NULL;
	if (searchflag == NULL) {
		nsearch = strdup("1 = 1");
	} else {
		if (strstr(searchflag, "upnp:class derivedfrom \"object.item.audio")) {
			nsearch = strdup("o.CLASS glob 'object.item.audio*'");
		} else if (strstr(searchflag, "upnp:class derivedfrom \"object.item.video")) {
			nsearch = strdup("o.CLASS glob 'object.item.video*'");
		} else if (strstr(searchflag, "upnp:class derivedfrom \"object.item.image")) {
			nsearch = strdup("o.CLASS glob 'object.item.image*'");
		} else {
			nsearch = strdup("1 = 1");
		}
	}
	sql = upnpd_sqlite3_mprintf(
			"SELECT count(*)"
			"  from OBJECT o"
			"  where o.PARENT glob '%s*' and %s;",
			parentid, nsearch);
	upnpd_sqlite3_exec(database->database, sql, database_count_callback, (void *) total, 0);
	upnpd_sqlite3_free(sql);
	if (*total == 0) {
		free(nsearch);
		return NULL;
	}
	sql = upnpd_sqlite3_mprintf(
			"SELECT o.ID,"
			"       o.CLASS,"
			"       o.PARENT,"
			"       d.PATH,"
			"       d.TITLE,"
			"       d.SIZE,"
			"       d.DURATION,"
			"       d.DATE,"
			"       d.MIME,"
			"       d.DLNA"
			"  from OBJECT o left join DETAIL d on (d.ID = o.DETAIL)"
			"  where o.PARENT glob '%s*' and %s order by d.TITLE limit %llu, %llu;",
			parentid,
			nsearch,
			start,
			count);
	darg.database = database->database;
	darg.args = (void *) &e;
	upnpd_sqlite3_exec(database->database, sql, database_query_callback, (void *) &darg, 0);
	upnpd_sqlite3_free(sql);
	if (e == NULL) {
		*total = 0;
	}
	free(nsearch);
	return e;
}

unsigned long long upnpd_database_insert (database_t *database,
		const char *class,
		const char *parentid,
		const char *path,
		const char *title,
		const unsigned long long size,
		const char *duration,
		const char *date,
		const char *mime,
		const char *dlna)
{
	char *sql;
	unsigned long long detailid;

	detailid = 0;
	sql = upnpd_sqlite3_mprintf(
			"INSERT into DETAIL"
			"  (PATH, TITLE, SIZE, DURATION, DATE, MIME, DLNA)"
			"  values"
			"  ('%s', '%s', %llu, '%s', '%s', '%s', '%s')",
			path,
			title,
			size,
			(duration) ? duration : "00:00:00.000",
			date,
			mime,
			dlna);
	upnpd_sqlite3_exec(database->database, sql, 0, 0, 0);
	upnpd_sqlite3_free(sql);

	detailid = upnpd_sqlite3_last_insert_rowid(database->database);

	sql = upnpd_sqlite3_mprintf(
			"INSERT into OBJECT"
			"  (ID, CLASS, PARENT, DETAIL)"
			"  values"
			"  ('%s$%llu', '%s', '%s', %llu)",
			parentid,
			detailid,
			class,
			parentid,
			detailid);
	upnpd_sqlite3_exec(database->database, sql, 0, 0, 0);
	upnpd_sqlite3_free(sql);

	debugf("inserted '%s' (%llu) under %s", path, detailid, parentid);

	return detailid;
}

int upnpd_database_entry_free (database_entry_t *entry)
{
	database_entry_t *n;
	database_entry_t *p;
	if (entry == NULL) {
		return 0;
	}
	n = entry;
	while (n) {
		p = n;
		n = n->next;
		free(p->date);
		free(p->dlna);
		free(p->duration);
		free(p->id);
		free(p->class);
		free(p->mime);
		free(p->parent);
		free(p->path);
		free(p->title);
		free(p);
	}
	return 0;
}
