/*
 * upnpavd - UPNP AV Daemon
 *
 * Copyright (C) 2009 Alper Akcan, alper.akcan@gmail.com
 * Copyright (C) 2010 Alper Akcan, alper.akcan@gmail.com
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#include "platform.h"
#include "metadata.h"
#include "database.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

static char entryid_convert (const char c)
{
	switch (c) {
		case 0 ... 9:
			return '0' + c;
		case 10 ... 15:
			return 'a' + c - 10;
		case '0' ... '9':
			return c - '0';
		case 'a' ... 'f':
			return c - 'a' + 10;
		case 'A' ... 'F':
			return c - 'A' + 10;
		default:
			debugf("logic error %x", c);
			assert(0);
	}
	return 0;
}

static char * entryid_path_from_id (const char *id)
{
	int i;
	int l;
	char *path;
	if (id == NULL) {
		return NULL;
	}
	l = strlen(id) / 2;
	path = (char *) malloc(l + 1);
	if (path == NULL) {
		return NULL;
	}
	memset(path, 0, l + 1);
	for (i = 0; i < l; i++) {
		path[i] = (entryid_convert(id[i * 2]) << 0x04) | entryid_convert(id[i * 2 + 1]);
	}
	debugf("entry path '%s'", path);
	return path;
}

char * entryid_id_from_path (const char *path)
{
	int i;
	int l;
	char *id;
	if (path == NULL) {
		return NULL;
	}
	l = strlen(path);
	id = (char *) malloc(l * 2 + 1);
	if (id == NULL) {
		return NULL;
	}
	memset(id, 0, l * 2 + 1);
	for (i = 0; i < l; i++) {
		id[i * 2]     = entryid_convert((path[i] >> 0x04) & 0xf);
		id[i * 2 + 1] = entryid_convert((path[i] >> 0x00) & 0xf);
	}
	return id;
}

static char * entryid_init_value (const char *path)
{
	char *id;
	id = entryid_id_from_path(path);
	if (id == NULL) {
		debugf("etnryid_id_from_path(); failed");
		return NULL;
	}
	return id;
}

static char * entryid_parentid_from_path (const char *path)
{
	char *p;
	char *parent;
	char *parentid;
	p = strrchr(path, '/');
	if (p == NULL || p == path) {
		parent = strdup("-1");
	} else {
		parent = (char *) malloc(p - path + 1);
		memset(parent, 0, p - path + 1);
		memcpy(parent, path, p - path);
	}
	parentid = entryid_id_from_path(parent);
	free(parent);
	return parentid;
}

static entry_t * entry_init_from_database (database_entry_t *dentry)
{
	entry_t *entry;
	if (dentry == NULL) {
		return NULL;
	}
	entry = (entry_t *) malloc(sizeof(entry_t));
	if (entry == NULL) {
		return NULL;
	}
	memset(entry, 0, sizeof(entry_t));
	if (strcmp(dentry->class, "object.container.storageFolder") == 0) {
		entry->didl.entryid = strdup(dentry->id);
		entry->didl.parentid = strdup(dentry->parent);
		entry->path = strdup(dentry->path);
		entry->didl.childcount = dentry->childs;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(dentry->title);
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER;
		entry->didl.upnp.object.class = strdup("object.container.storageFolder");
		entry->didl.upnp.storagefolder.storageused = 0;
	} else if (strcmp(dentry->class, "object.item.audioItem.musicTrack") == 0) {
		entry->didl.entryid = strdup(dentry->id);
		entry->didl.parentid = strdup(dentry->parent);
		entry->path = strdup(dentry->path);
		entry->mime = strdup(dentry->mime);
		entry->ext_info = strdup(dentry->dlna);
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(dentry->title);
		entry->didl.dc.contributor = NULL;
		entry->didl.dc.date = strdup(dentry->date);
		entry->didl.dc.description = NULL;
		entry->didl.dc.publisher = NULL;
		entry->didl.dc.language = NULL;
		entry->didl.dc.relation = NULL;
		entry->didl.dc.rights = NULL;
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_MUSICTRACK;
		entry->didl.upnp.object.class = strdup("object.item.audioItem.musicTrack");
		entry->didl.upnp.musictrack.artist = NULL;
		entry->didl.upnp.musictrack.album = NULL;
		entry->didl.upnp.musictrack.originaltracknumber = 0;
		entry->didl.upnp.musictrack.playlist = NULL;
		entry->didl.upnp.musictrack.audioitem.genre = NULL;
		entry->didl.upnp.musictrack.audioitem.longdescription = NULL;
		asprintf(&entry->didl.res.protocolinfo, "http-get:*:%s:%s", entry->mime, entry->ext_info);
		entry->didl.res.size = dentry->size;
	} else if (strcmp(dentry->class, "object.item.videoItem.movie") == 0) {
		entry->didl.entryid = strdup(dentry->id);
		entry->didl.parentid = strdup(dentry->parent);
		entry->path = strdup(dentry->path);
		entry->mime = strdup(dentry->mime);
		entry->ext_info = strdup(dentry->dlna);
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(dentry->title);
		entry->didl.dc.contributor = NULL;
		entry->didl.dc.date = strdup(dentry->date);
		entry->didl.dc.description = NULL;
		entry->didl.dc.publisher = NULL;
		entry->didl.dc.language = NULL;
		entry->didl.dc.relation = NULL;
		entry->didl.dc.rights = NULL;
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_MOVIE;
		entry->didl.upnp.object.class = strdup("object.item.videoItem.movie");
		entry->didl.upnp.movie.videoitem.actor = NULL;
		entry->didl.upnp.movie.videoitem.director = NULL;
		entry->didl.upnp.movie.videoitem.genre = NULL;
		entry->didl.upnp.movie.videoitem.longdescription = NULL;
		entry->didl.upnp.movie.videoitem.producer = NULL;
		entry->didl.upnp.movie.videoitem.rating = NULL;
		asprintf(&entry->didl.res.protocolinfo, "http-get:*:%s:%s", entry->mime, entry->ext_info);
		entry->didl.res.size = dentry->size;
	} else if (strcmp(dentry->class, "object.item.imageItem.photo") == 0) {
		entry->didl.entryid = strdup(dentry->id);
		entry->didl.parentid = strdup(dentry->parent);
		entry->path = strdup(dentry->path);
		entry->mime = strdup(dentry->mime);
		entry->ext_info = strdup(dentry->dlna);
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(dentry->title);
		entry->didl.dc.contributor = NULL;
		entry->didl.dc.date = strdup(dentry->date);
		entry->didl.dc.description = NULL;
		entry->didl.dc.publisher = NULL;
		entry->didl.dc.language = NULL;
		entry->didl.dc.relation = NULL;
		entry->didl.dc.rights = NULL;
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_PHOTO;
		entry->didl.upnp.object.class = strdup("object.item.imageItem.photo");
		entry->didl.upnp.photo.imageitem.longdescription = NULL;
		entry->didl.upnp.photo.imageitem.rating = NULL;
		entry->didl.upnp.photo.imageitem.storagemedium = NULL;
		entry->didl.upnp.photo.album = NULL;
		asprintf(&entry->didl.res.protocolinfo, "http-get:*:%s:%s", entry->mime, entry->ext_info);
		entry->didl.res.size = dentry->size;
	} else {
		free(entry);
		return NULL;
	}
	return entry;
}

entry_t * entry_didl_from_id (void *database, const char *id)
{
	char *path;
	entry_t *entry;
	database_t *db;
	database_entry_t *de;
	db = (database_t *) database;
	if (db == NULL) {
		path = entryid_path_from_id(id);
		entry = entry_didl_from_path(path);
		free(path);
		return entry;
	} else {
		de = database_query_entry(db, id);
		entry = entry_init_from_database(de);
		database_entry_free(de);
		return entry;
	}
}

entry_t * entry_didl_from_path (const char *path)
{
	entry_t *entry;
	metadata_t *metadata;
	metadata = metadata_init(path);
	if (metadata == NULL) {
		debugf("metadata_init('%s') failed", path);
		return NULL;
	}
	entry = (entry_t *) malloc(sizeof(entry_t));
	if (entry == NULL) {
		metadata_uninit(metadata);
		return NULL;
	}
	memset(entry, 0, sizeof(entry_t));
	if (metadata->type == METADATA_TYPE_CONTAINER) {
		entry->didl.entryid = entryid_init_value(path);
		if (entry->didl.entryid == NULL) {
			goto error;
		}
		entry->didl.parentid = entryid_parentid_from_path(path);
		entry->path = strdup(metadata->pathname);
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(metadata->basename);
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER;
		entry->didl.upnp.object.class = strdup("object.container.storageFolder");
		entry->didl.upnp.storagefolder.storageused = 0;
	} else if (metadata->type == METADATA_TYPE_AUDIO) {
		entry->didl.entryid = entryid_init_value(path);
		if (entry->didl.entryid == NULL) {
			goto error;
		}
		entry->didl.parentid = entryid_parentid_from_path(path);
		entry->path = strdup(metadata->pathname);
		entry->mime = strdup(metadata->mimetype);
		entry->ext_info = strdup("*");
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(metadata->basename);
		entry->didl.dc.contributor = NULL;
		entry->didl.dc.date = strdup(metadata->date);
		entry->didl.dc.description = NULL;
		entry->didl.dc.publisher = NULL;
		entry->didl.dc.language = NULL;
		entry->didl.dc.relation = NULL;
		entry->didl.dc.rights = NULL;
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_MUSICTRACK;
		entry->didl.upnp.object.class = strdup("object.item.audioItem.musicTrack");
		entry->didl.upnp.musictrack.artist = NULL;
		entry->didl.upnp.musictrack.album = NULL;
		entry->didl.upnp.musictrack.originaltracknumber = 0;
		entry->didl.upnp.musictrack.playlist = NULL;
		entry->didl.upnp.musictrack.audioitem.genre = NULL;
		entry->didl.upnp.musictrack.audioitem.longdescription = NULL;
		asprintf(&entry->didl.res.protocolinfo, "http-get:*:%s:%s", entry->mime, entry->ext_info);
		entry->didl.res.size = metadata->size;
	} else if (metadata->type == METADATA_TYPE_VIDEO) {
		entry->didl.entryid = entryid_init_value(path);
		if (entry->didl.entryid == NULL) {
			goto error;
		}
		entry->didl.parentid = entryid_parentid_from_path(path);
		entry->path = strdup(metadata->pathname);
		entry->mime = strdup(metadata->mimetype);
		entry->ext_info = strdup("*");
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(metadata->basename);
		entry->didl.dc.contributor = NULL;
		entry->didl.dc.date = strdup(metadata->date);
		entry->didl.dc.description = NULL;
		entry->didl.dc.publisher = NULL;
		entry->didl.dc.language = NULL;
		entry->didl.dc.relation = NULL;
		entry->didl.dc.rights = NULL;
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_MOVIE;
		entry->didl.upnp.object.class = strdup("object.item.videoItem.movie");
		entry->didl.upnp.movie.videoitem.actor = NULL;
		entry->didl.upnp.movie.videoitem.director = NULL;
		entry->didl.upnp.movie.videoitem.genre = NULL;
		entry->didl.upnp.movie.videoitem.longdescription = NULL;
		entry->didl.upnp.movie.videoitem.producer = NULL;
		entry->didl.upnp.movie.videoitem.rating = NULL;
		asprintf(&entry->didl.res.protocolinfo, "http-get:*:%s:%s", entry->mime, entry->ext_info);
		entry->didl.res.size = metadata->size;
	} else if (metadata->type == METADATA_TYPE_IMAGE) {
		entry->didl.entryid = entryid_init_value(path);
		if (entry->didl.entryid == NULL) {
			goto error;
		}
		entry->didl.parentid = entryid_parentid_from_path(path);
		entry->path = strdup(metadata->pathname);
		entry->mime = strdup(metadata->mimetype);
		entry->ext_info = strdup("*");
		entry->didl.childcount = 0;
		entry->didl.restricted = 1;
		entry->didl.dc.title = strdup(metadata->basename);
		entry->didl.dc.contributor = NULL;
		entry->didl.dc.date = strdup(metadata->date);
		entry->didl.dc.description = NULL;
		entry->didl.dc.publisher = NULL;
		entry->didl.dc.language = NULL;
		entry->didl.dc.relation = NULL;
		entry->didl.dc.rights = NULL;
		entry->didl.upnp.type = DIDL_UPNP_OBJECT_TYPE_PHOTO;
		entry->didl.upnp.object.class = strdup("object.item.imageItem.photo");
		entry->didl.upnp.photo.imageitem.longdescription = NULL;
		entry->didl.upnp.photo.imageitem.rating = NULL;
		entry->didl.upnp.photo.imageitem.storagemedium = NULL;
		entry->didl.upnp.photo.album = NULL;
		asprintf(&entry->didl.res.protocolinfo, "http-get:*:%s:%s", entry->mime, entry->ext_info);
		entry->didl.res.size = metadata->size;
	} else {
		goto error;
	}
	metadata_uninit(metadata);
	return entry;
error:	entry_uninit(entry);
	metadata_uninit(metadata);
	return NULL;
}

int entry_print (entry_t *entry)
{
	entry_t *c;
	if (entry == NULL) {
		return 0;
	}
	c = entry;
	while (c) {
		if (c->didl.upnp.type == DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER) {
			printf("%s - %s (class: %s, size:%u, childs: %u)\n", c->didl.entryid, c->didl.dc.title, c->didl.upnp.object.class, c->didl.upnp.storagefolder.storageused, c->didl.childcount);
		} else {
			printf("%s - %s (class: %s, size:%llu)\n", c->didl.entryid, c->didl.dc.title, c->didl.upnp.object.class, c->didl.res.size);
		}
		c = c->next;
	}
	return 0;
}

int entry_dump (entry_t *entry)
{
	entry_t *c;
	if (entry == NULL) {
		return 0;
	}
	c = entry;
	while (c) {
		printf("%s - %s\n", c->didl.entryid, c->didl.dc.title);
		printf("  didl.id        : %s\n", c->didl.entryid);
		printf("  didl.parentid  : %s\n", c->didl.parentid);
		printf("  didl.childcount: %u\n", c->didl.childcount);
		printf("  didl.restricted: %d\n", c->didl.restricted);
		printf("  didl.dc.title      : %s\n", c->didl.dc.title);
		printf("  didl.dc.creator    : %s\n", c->didl.dc.creator);
		printf("  didl.dc.subject    : %s\n", c->didl.dc.subject);
		printf("  didl.dc.description: %s\n", c->didl.dc.description);
		printf("  didl.dc.publisher  : %s\n", c->didl.dc.publisher);
		printf("  didl.dc.contributor: %s\n", c->didl.dc.contributor);
		printf("  didl.dc.date       : %s\n", c->didl.dc.date);
		printf("  didl.dc.type       : %s\n", c->didl.dc.type);
		printf("  didl.dc.format     : %s\n", c->didl.dc.format);
		printf("  didl.dc.identifier : %s\n", c->didl.dc.identifier);
		printf("  didl.dc.source     : %s\n", c->didl.dc.source);
		printf("  didl.dc.language   : %s\n", c->didl.dc.language);
		printf("  didl.dc.relation   : %s\n", c->didl.dc.relation);
		printf("  didl.dc.coverage   : %s\n", c->didl.dc.coverage);
		printf("  didl.dc.rights     : %s\n", c->didl.dc.rights);
		printf("  didl.upnp.object.class : %s\n", c->didl.upnp.object.class);
		switch (c->didl.upnp.type) {
			case DIDL_UPNP_OBJECT_TYPE_MUSICTRACK:
				printf("  didl.upnp.artist             : %s\n", c->didl.upnp.musictrack.artist);
				printf("  didl.upnp.album              : %s\n", c->didl.upnp.musictrack.album);
				printf("  didl.upnp.originaltracknumber: %u\n", c->didl.upnp.musictrack.originaltracknumber);
				printf("  didl.upnp.playlist           : %s\n", c->didl.upnp.musictrack.playlist);
			case DIDL_UPNP_OBJECT_TYPE_AUDIOITEM:
				printf("  didl.upnp.genre              : %s\n", c->didl.upnp.audioitem.genre);
				printf("  didl.upnp.longdescription    : %s\n", c->didl.upnp.audioitem.longdescription);
				break;
			case DIDL_UPNP_OBJECT_TYPE_MOVIE:
			case DIDL_UPNP_OBJECT_TYPE_VIDEOITEM:
				printf("  didl.upnp.actor              : %s\n", c->didl.upnp.videoitem.actor);
				printf("  didl.upnp.genre              : %s\n", c->didl.upnp.videoitem.genre);
				printf("  didl.upnp.longdescription    : %s\n", c->didl.upnp.videoitem.longdescription);
				printf("  didl.upnp.producer           : %s\n", c->didl.upnp.videoitem.producer);
				printf("  didl.upnp.rating             : %s\n", c->didl.upnp.videoitem.rating);
				printf("  didl.upnp.director           : %s\n", c->didl.upnp.videoitem.director);
				break;
			case DIDL_UPNP_OBJECT_TYPE_PHOTO:
				printf("  didl.upnp.photo.album        : %s\n", c->didl.upnp.photo.album);
			case DIDL_UPNP_OBJECT_TYPE_IMAGEITEM:
				printf("  didl.upnp.longdescription    : %s\n", c->didl.upnp.imageitem.longdescription);
				printf("  didl.upnp.rating             : %s\n", c->didl.upnp.imageitem.rating);
				printf("  didl.upnp.storagemedium      : %s\n", c->didl.upnp.imageitem.storagemedium);
				break;
			case DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER:
				printf("  didl.upnp.storageused        : %u\n", c->didl.upnp.storagefolder.storageused);
				break;
			default:
				break;
		}
		printf("  didl.res.protocolinfo: %s\n", c->didl.res.protocolinfo);
		printf("  didl.res.size        : %llu\n", c->didl.res.size);
		printf("  didl.res.path        : %s\n", c->didl.res.path);
		c = c->next;
	}
	return 0;
}

static int entry_scan_path (database_t *database, const char *path, const char *parentid)
{
	char *tmp;
	char *ptr;
	dir_t *dir;
	entry_t *entry;
	dir_entry_t *current;
	unsigned long long size;
	unsigned long long objectid;

	current = (dir_entry_t *) malloc(sizeof(dir_entry_t));
	if (current == NULL) {
		return -1;
	}
	dir = file_opendir(path);
	if (dir == NULL) {
		free(current);
		return -1;
	}
	debugf("looking into: %s", path);
	while (file_readdir(dir, current) == 0) {
		if (strncmp(current->name, ".", 1) == 0) {
			/* will cover parent, self, hidden */
			continue;
		}
		if (asprintf(&ptr, "%s/%s", path, current->name) < 0) {
			continue;
		}
		entry = entry_didl_from_path(ptr);
		if (entry == NULL) {
			debugf("entry_didl_from_path(%s, %s) failed", ptr, path);
			free(ptr);
			continue;
		}
		debugf("found: %s", entry->path);
		if (entry->didl.upnp.type != DIDL_UPNP_OBJECT_TYPE_UNKNOWN) {
			size = entry->didl.res.size;
			objectid = database_insert(database,
					entry->didl.upnp.object.class,
					parentid,
					entry->path,
					entry->didl.dc.title,
					size,
					entry->didl.dc.date,
					entry->mime,
					"*");
			if (entry->didl.upnp.type == DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER) {
				if (asprintf(&tmp, "%s$%llu", parentid, objectid) > 0) {
					entry_scan_path(database, entry->path, tmp);
					free(tmp);
				}
			}
		}
		entry_uninit(entry);
		free(ptr);
	}
	free(current);
	file_closedir(dir);
	return 0;
}

void * entry_scan (const char *path)
{
	int ret;
	database_t *database;
	database = database_init(1);
	ret = entry_scan_path(database, path, "0");
	database_index(database);
	return (void *) database;
}

entry_t * entry_init_from_id (void *database, const char *id, unsigned int start, unsigned int count, unsigned int *returned, unsigned int *total)
{
	char *path;
	entry_t *entry;
	entry_t *tentry;
	database_t *db;
	database_entry_t *de;
	database_entry_t *dt;
	unsigned long long ds;
	db = (database_t *) database;
	if (db == NULL) {
		path = entryid_path_from_id(id);
		entry = entry_init_from_path(path, start, count, returned, total);
		free(path);
		return entry;
	} else {
		de = database_query_parent(db, id, start, count, &ds);
		*total = ds;
		if (*total == 0) {
			return NULL;
		}
		entry = NULL;
		tentry = NULL;
		for (dt = de; dt != NULL; dt = dt->next) {
			if (tentry == NULL) {
				tentry = entry_init_from_database(dt);
				if (tentry == NULL) {
					break;
				}
				entry = tentry;
			} else {
				tentry->next = entry_init_from_database(dt);
				if (tentry->next == NULL) {
					break;
				}
				tentry = tentry->next;
			}
		}
		*returned = 0;
		tentry = entry;
		while (tentry != NULL) {
			*returned = *returned + 1;
			tentry = tentry->next;
		}
		database_entry_free(de);
		debugf("returned: %d, total: %d\n", *returned, *total);
		tentry = entry;
		while (tentry != NULL) {
			tentry = tentry->next;
		}
		return entry;
	}
}

entry_t * entry_init_from_search (void *database, const char *id, unsigned int start, unsigned int count, unsigned int *returned, unsigned int *total, const char *serach)
{
	entry_t *entry;
	entry_t *tentry;
	database_t *db;
	database_entry_t *de;
	database_entry_t *dt;
	unsigned long long ds;
	db = (database_t *) database;
	if (db == NULL) {
		debugf("search is not supported without database");
		return NULL;
	} else {
		de = database_query_search(db, id, start, count, &ds, serach);
		*total = ds;
		if (*total == 0) {
			return NULL;
		}
		entry = NULL;
		tentry = NULL;
		for (dt = de; dt != NULL; dt = dt->next) {
			if (tentry == NULL) {
				tentry = entry_init_from_database(dt);
				if (tentry == NULL) {
					break;
				}
				entry = tentry;
			} else {
				tentry->next = entry_init_from_database(dt);
				if (tentry->next == NULL) {
					break;
				}
				tentry = tentry->next;
			}
		}
		*returned = 0;
		tentry = entry;
		while (tentry != NULL) {
			*returned = *returned + 1;
			tentry = tentry->next;
		}
		database_entry_free(de);
		debugf("returned: %d, total: %d\n", *returned, *total);
		tentry = entry;
		while (tentry != NULL) {
			tentry = tentry->next;
		}
		return entry;
	}
}

entry_t * entry_init_from_path (const char *path, unsigned int start, unsigned int count, unsigned int *returned, unsigned int *total)
{
	char *ptr;
	dir_t *dir;
	entry_t *tmp;
	entry_t *next;
	entry_t *prev;
	entry_t *entry;
	dir_entry_t *current;

	next = NULL;
	entry = NULL;
	*total = 0;

	current = (dir_entry_t *) malloc(sizeof(dir_entry_t));
	if (current == NULL) {
		return NULL;
	}
	dir = file_opendir(path);
	if (dir == NULL) {
		free(current);
		return NULL;
	}
	debugf("looking into: %s", path);
	while (file_readdir(dir, current) == 0) {
		if (strncmp(current->name, ".", 1) == 0) {
			/* will cover parent, self, hidden */
			continue;
		}
		if (asprintf(&ptr, "%s/%s", path, current->name) < 0) {
			continue;
		}
		next = entry_didl_from_path(ptr);
		if (next == NULL) {
			debugf("entry_didl_from_path(%s, %s) failed", ptr, path);
			free(ptr);
			continue;
		}
		debugf("found: %s, %s, 0x%08x", next->didl.dc.title, next->didl.entryid, next->didl.upnp.type);
		if (entry == NULL) {
			entry = next;
		} else {
			tmp = entry;
			prev = NULL;
			while (tmp != NULL) {
				if ((next->didl.upnp.type == tmp->didl.upnp.type && (strcmp(next->didl.dc.title, tmp->didl.dc.title) < 0)) ||
				    (next->didl.upnp.type != tmp->didl.upnp.type && next->didl.upnp.type == DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER)) {
					if (tmp == entry) {
						next->next = entry;
						entry = next;
					} else {
						next->next = tmp;
						prev->next = next;
					}
					goto found;
				}
				prev = tmp;
				tmp = tmp->next;
			}
			prev->next = next;
		}
found:
		free(ptr);
		*total = *total + 1;
	}
	free(current);
	file_closedir(dir);

	while (entry != NULL && start > 0) {
		tmp = entry;
		entry = tmp->next;
		tmp->next = NULL;
		entry_uninit(tmp);
		start--;
	}
	tmp = entry;
	prev = NULL;
	*returned = 0;
	while (tmp != NULL && count > 0) {
		prev = tmp;
		tmp = tmp->next;
		count--;
		*returned = *returned + 1;
	}
	if (prev != NULL) {
		prev->next = NULL;
	}
	if (tmp != NULL) {
		entry_uninit(tmp);
	}

	return entry;
}

int entry_uninit (entry_t *root)
{
	entry_t *n;
	entry_t *p;
	if (root == NULL) {
		return 0;
	}
	n = root;
	while (n) {
		p = n;
		n = n->next;
		free(p->mime);
		free(p->path);
		free(p->ext_info);
		free(p->metadata);
		free(p->didl.entryid);
		free(p->didl.parentid);
		free(p->didl.dc.title);
		free(p->didl.dc.contributor);
		free(p->didl.dc.coverage);
		free(p->didl.dc.creator);
		free(p->didl.dc.date);
		free(p->didl.dc.description);
		free(p->didl.dc.publisher);
		free(p->didl.dc.language);
		free(p->didl.dc.relation);
		free(p->didl.dc.rights);
		free(p->didl.upnp.object.class);
		switch (p->didl.upnp.type) {
			case DIDL_UPNP_OBJECT_TYPE_MUSICTRACK:
				free(p->didl.upnp.musictrack.artist);
				free(p->didl.upnp.musictrack.album);
				free(p->didl.upnp.musictrack.playlist);
			case DIDL_UPNP_OBJECT_TYPE_AUDIOITEM:
				free(p->didl.upnp.audioitem.genre);
				free(p->didl.upnp.audioitem.longdescription);
				break;
			case DIDL_UPNP_OBJECT_TYPE_MOVIE:
			case DIDL_UPNP_OBJECT_TYPE_VIDEOITEM:
				free(p->didl.upnp.videoitem.actor);
				free(p->didl.upnp.videoitem.director);
				free(p->didl.upnp.videoitem.genre);
				free(p->didl.upnp.videoitem.longdescription);
				free(p->didl.upnp.videoitem.producer);
				free(p->didl.upnp.videoitem.rating);
				break;
			case DIDL_UPNP_OBJECT_TYPE_PHOTO:
				free(p->didl.upnp.photo.album);
			case DIDL_UPNP_OBJECT_TYPE_IMAGEITEM:
				free(p->didl.upnp.photo.imageitem.longdescription);
				free(p->didl.upnp.photo.imageitem.rating);
				free(p->didl.upnp.photo.imageitem.storagemedium);
				break;
			case DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER:
				break;
			default:
				break;
		}
		free(p->didl.res.protocolinfo);
		free(p->didl.res.duration);
		free(p->didl.res.path);
		free(p);
	}
	return 0;
}

static didl_upnp_object_type_t entry_upnp_type_from_class (const char *class)
{
	if (class == NULL) {
		return DIDL_UPNP_OBJECT_TYPE_UNKNOWN;
	} else if (strcmp(class, "object.container.storageFolder") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER;
	} else if (strcmp(class, "object.item.audioItem") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_AUDIOITEM;
	} else if (strcmp(class, "object.item.audioItem.musicTrack") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_MUSICTRACK;
	} else if (strcmp(class, "object.item.videoItem") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_VIDEOITEM;
	} else if (strcmp(class, "object.item.videoItem.movie") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_MOVIE;
	} else if (strcmp(class, "object.item.imageItem") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_IMAGEITEM;
	} else if (strcmp(class, "object.item.imageItem.photo") == 0) {
		return DIDL_UPNP_OBJECT_TYPE_PHOTO;
	} else {
		return DIDL_UPNP_OBJECT_TYPE_UNKNOWN;
	}
}

typedef struct entry_parser_data_s {
	entry_t *curr;
	entry_t *prev;
	entry_t *root;
} entry_parser_data_t;

static int entry_parser_callback (void *context, const char *path, const char *name, const char **atrr, const char *value)
{
	int a;
	entry_parser_data_t *data;
	data = (entry_parser_data_t *) context;
	if (strcmp(path, "/DIDL-Lite/item") == 0 ||
	    strcmp(path, "/DIDL-Lite/container") == 0) {
		data->curr = (entry_t *) malloc(sizeof(entry_t));
		if (data->curr != NULL) {
			memset(data->curr, 0, sizeof(entry_t));
			for (a = 0; atrr[a] && atrr[a + 1]; a += 2) {
				if (strcmp(atrr[a], "id") == 0) {
					data->curr->didl.entryid = strdup(atrr[a + 1]);
				} else if (strcmp(atrr[a], "parentID") == 0) {
					data->curr->didl.parentid = strdup(atrr[a + 1]);
				} else if (strcmp(atrr[a], "childCount") == 0) {
					data->curr->didl.childcount = strtoint32(atrr[a + 1]);
				} else if (strcmp(atrr[a], "restricted") == 0) {
					data->curr->didl.restricted = strtoint32(atrr[a + 1]);
				}
			}
			if (data->curr->didl.entryid && data->curr->didl.parentid) {
				if (data->prev == NULL) {
					data->root = data->curr;
				} else {
					data->prev->next = data->curr;
				}
				data->prev = data->curr;
			} else {
				free(data->curr->didl.entryid);
				free(data->curr->didl.parentid);
				free(data->curr);
				data->curr = NULL;
			}
		}
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:contributor") == 0) {
		data->curr->didl.dc.contributor = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:coverage") == 0) {
		data->curr->didl.dc.coverage = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:creator") == 0) {
		data->curr->didl.dc.creator = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:date") == 0) {
		data->curr->didl.dc.date = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:description") == 0) {
		data->curr->didl.dc.description = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:format") == 0) {
		data->curr->didl.dc.format = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:identifier") == 0) {
		data->curr->didl.dc.identifier = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:language") == 0) {
		data->curr->didl.dc.language = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:publisher") == 0) {
		data->curr->didl.dc.publisher = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:relation") == 0) {
		data->curr->didl.dc.relation = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:rights") == 0) {
		data->curr->didl.dc.rights = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:source") == 0) {
		data->curr->didl.dc.source = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:subject") == 0) {
		data->curr->didl.dc.subject = strdup(value);
	} else if (value != NULL && data->curr != NULL && (strcmp(path, "/DIDL-Lite/item/dc:title") == 0 || strcmp(path, "/DIDL-Lite/container/dc:title") == 0)) {
		data->curr->didl.dc.title = strdup(value);
	} else if (value != NULL && data->curr != NULL && strcmp(path, "/DIDL-Lite/item/dc:type") == 0) {
		data->curr->didl.dc.type = strdup(value);
	} else if (value != NULL && data->curr != NULL && (strcmp(path, "/DIDL-Lite/item/upnp:class") == 0 || strcmp(path, "/DIDL-Lite/container/upnp:class") == 0)) {
		data->curr->didl.upnp.object.class = strdup(value);
		data->curr->didl.upnp.type = entry_upnp_type_from_class(value);
	}
	if (value != NULL && data->curr != NULL) {
		switch (data->curr->didl.upnp.type) {
			case DIDL_UPNP_OBJECT_TYPE_MUSICTRACK:
				if (strcmp(path, "/DIDL-Lite/item/upnp:album") == 0) {
					data->curr->didl.upnp.musictrack.album = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:artist") == 0) {
					data->curr->didl.upnp.musictrack.artist = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:originalTrackNumber") == 0) {
					data->curr->didl.upnp.musictrack.originaltracknumber = strtouint32(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:playlist") == 0) {
					data->curr->didl.upnp.musictrack.playlist = strdup(value);
				}
			case DIDL_UPNP_OBJECT_TYPE_AUDIOITEM:
				if (strcmp(path, "/DIDL-Lite/item/upnp:genre") == 0) {
					data->curr->didl.upnp.audioitem.genre= strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:longDescription") == 0) {
					data->curr->didl.upnp.audioitem.longdescription = strdup(value);
				}
				break;
			case DIDL_UPNP_OBJECT_TYPE_MOVIE:
			case DIDL_UPNP_OBJECT_TYPE_VIDEOITEM:
				if (strcmp(path, "/DIDL-Lite/item/upnp:actor") == 0) {
					data->curr->didl.upnp.videoitem.actor = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:director") == 0) {
					data->curr->didl.upnp.videoitem.director = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:genre") == 0) {
					data->curr->didl.upnp.videoitem.genre = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:longdescription") == 0) {
					data->curr->didl.upnp.videoitem.longdescription = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:producer") == 0) {
					data->curr->didl.upnp.videoitem.producer = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:rating") == 0) {
					data->curr->didl.upnp.videoitem.rating = strdup(value);
				}
				break;
			case DIDL_UPNP_OBJECT_TYPE_PHOTO:
				if (strcmp(path, "/DIDL-Lite/item/upnp:album") == 0) {
					data->curr->didl.upnp.photo.album = strdup(value);
				}
			case DIDL_UPNP_OBJECT_TYPE_IMAGEITEM:
				if (strcmp(path, "/DIDL-Lite/item/upnp:longdescription") == 0) {
					data->curr->didl.upnp.imageitem.longdescription = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:rating") == 0) {
					data->curr->didl.upnp.imageitem.rating = strdup(value);
				} else if (strcmp(path, "/DIDL-Lite/item/upnp:storagemedium") == 0) {
					data->curr->didl.upnp.imageitem.storagemedium = strdup(value);
				}
				break;
			case DIDL_UPNP_OBJECT_TYPE_STORAGEFOLDER:
				if (strcmp(path, "/DIDL-Lite/container/upnp:storageUsed") == 0) {
					data->curr->didl.upnp.storagefolder.storageused = strtoint32(value);
				}
				break;
			default:
				break;
		}
		if (strcmp(path, "/DIDL-Lite/item/res") == 0) {
			if (strncmp(value, "http://", 7) == 0 && data->curr->didl.res.path == NULL) {
				for (a = 0; atrr[a] && atrr[a + 1]; a += 2) {
					if (strcmp(atrr[a], "protocolInfo") == 0) {
						free(data->curr->didl.res.protocolinfo);
						data->curr->didl.res.protocolinfo = strdup(atrr[a + 1]);
					} else if (strcmp(atrr[a], "duration") == 0) {
						free(data->curr->didl.res.duration);
						data->curr->didl.res.duration = strdup(atrr[a + 1]);
					} else if (strcmp(atrr[a], "size") == 0) {
						data->curr->didl.res.size = atoll(atrr[a + 1]);
					}
				}
				free(data->curr->didl.res.path);
				data->curr->didl.res.path = strdup(value);
			}
		}
	}
	return 0;
}

entry_t * entry_from_result (const char *result)
{
	entry_parser_data_t data;
	memset(&data, 0, sizeof(data));
	if (xml_parse_buffer_callback(result, strlen(result), entry_parser_callback, &data) != 0) {
		debugf("xml_parse_buffer_callback(result) failed\n");
	}
	return data.root;
}

char * entry_to_result (device_service_t *service, entry_t *entry, int metadata)
{
	int rc;
	char *out;
	char *ptr;
	char *tmp;
	char *id;
	char *pid;
	char *title;
	char *album;
	char *artist;
	char *genre;
	char *path;
	static char *didl =
		"<DIDL-Lite"
		" xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
		" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\""
		" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">";
	static char *didk =
		"</DIDL-Lite>";

	out = NULL;
	rc = asprintf(&out, "%s", didl);
	if (rc < 0) {
		return NULL;
	}
	while (entry) {
		//entry_dump(entry);
		id = xml_escape(entry->didl.entryid, 1);
		pid = xml_escape(entry->didl.parentid, 1);
		title = xml_escape(entry->didl.dc.title, 0);
		path = uri_escape(entry->didl.entryid);
		if (strcmp(entry->didl.upnp.object.class, "object.container.storageFolder") == 0) {
			static char *cfmt =
				"<container id=\"%s\" parentID=\"%s\" childCount=\"%u\" restricted=\"%s\" searchable=\"%s\">"
				" <dc:title>%s</dc:title>"
				" <upnp:class>%s</upnp:class>"
				" <upnp:storageUsed>%u</upnp:storageUsed>"
				"</container>";
			rc = asprintf(&tmp, cfmt,
				id, pid, entry->didl.childcount, (entry->didl.restricted == 1) ? "true" : "false", "false",
				title,
				entry->didl.upnp.object.class,
				entry->didl.upnp.storagefolder.storageused);
		} else if (strcmp(entry->didl.upnp.object.class, "object.item.audioItem.musicTrack") == 0) {
			static char *ifmt =
				"<item id=\"%s\" parentID=\"%s\" restricted=\"%s\">"
				" <dc:title>%s</dc:title>"
				" <dc:contributor>%s</dc:contributor>"
				" <dc:date>%s</dc:date>"
				" <dc:description>%s</dc:description>"
				" <dc:publisher>%s</dc:publisher>"
				" <dc:language>%s</dc:language>"
				" <dc:relation>%s</dc:relation>"
				" <dc:rights>%s</dc:rights>"
				" <upnp:class>%s</upnp:class>"
				" <upnp:artist>%s</upnp:artist>"
				" <upnp:album>%s</upnp:album>"
				" <upnp:originalTrackNumber>%u</upnp:originalTrackNumber>"
				" <upnp:playlist>%s</upnp:playlist>"
				" <upnp:genre>%s</upnp:genre>"
				" <upnp:longDescription>%s</upnp:longDescription>"
				" <res protocolInfo=\"%s\" size=\"%llu\">http://%s:%d/upnp/contentdirectory?id=%s</res>"
				"</item>";
			album = xml_escape(entry->didl.upnp.musictrack.album, 0);
			artist = xml_escape(entry->didl.upnp.musictrack.artist, 0);
			genre = xml_escape(entry->didl.upnp.audioitem.genre, 0);
			rc = asprintf(&tmp, ifmt,
				id, pid, (entry->didl.restricted == 1) ? "true" : "false",
				title,
				entry->didl.dc.contributor,
				entry->didl.dc.date,
				entry->didl.dc.description,
				entry->didl.dc.publisher,
				entry->didl.dc.language,
				entry->didl.dc.relation,
				entry->didl.dc.rights,
				entry->didl.upnp.object.class,
				artist,
				album,
				entry->didl.upnp.musictrack.originaltracknumber,
				entry->didl.upnp.musictrack.playlist,
				genre,
				entry->didl.upnp.audioitem.longdescription,
				entry->didl.res.protocolinfo, entry->didl.res.size, upnp_getaddress(service->device->upnp), upnp_getport(service->device->upnp), path);
			free(album);
			free(artist);
			free(genre);
		} else if (strcmp(entry->didl.upnp.object.class, "object.item.videoItem.movie") == 0) {
			static char *ifmt =
				"<item id=\"%s\" parentID=\"%s\" restricted=\"%s\">"
				" <dc:title>%s</dc:title>"
				" <dc:contributor>%s</dc:contributor>"
				" <dc:date>%s</dc:date>"
				" <dc:description>%s</dc:description>"
				" <dc:publisher>%s</dc:publisher>"
				" <dc:language>%s</dc:language>"
				" <dc:relation>%s</dc:relation>"
				" <dc:rights>%s</dc:rights>"
				" <upnp:class>%s</upnp:class>"
				" <upnp:genre>%s</upnp:genre>"
				" <upnp:longDescription>%s</upnp:longDescription>"
				" <upnp:producer>%u</upnp:producer>"
				" <upnp:rating>%s</upnp:rating>"
				" <upnp:actor>%s</upnp:actor>"
				" <upnp:director>%s</upnp:director>"
				" <res protocolInfo=\"%s\" size=\"%llu\">http://%s:%d/upnp/contentdirectory?id=%s</res>"
				"</item>";
			rc = asprintf(&tmp, ifmt,
				id, pid, (entry->didl.restricted == 1) ? "true" : "false",
				title,
				entry->didl.dc.contributor,
				entry->didl.dc.date,
				entry->didl.dc.description,
				entry->didl.dc.publisher,
				entry->didl.dc.language,
				entry->didl.dc.relation,
				entry->didl.dc.rights,
				entry->didl.upnp.object.class,
				entry->didl.upnp.videoitem.genre,
				entry->didl.upnp.videoitem.longdescription,
				entry->didl.upnp.videoitem.producer,
				entry->didl.upnp.videoitem.rating,
				entry->didl.upnp.videoitem.actor,
				entry->didl.upnp.videoitem.director,
				entry->didl.res.protocolinfo, entry->didl.res.size, upnp_getaddress(service->device->upnp), upnp_getport(service->device->upnp), path);
		} else if (strcmp(entry->didl.upnp.object.class, "object.item.imageItem.photo") == 0) {
			static char *ifmt =
				"<item id=\"%s\" parentID=\"%s\" restricted=\"%s\">"
				" <dc:title>%s</dc:title>"
				" <dc:contributor>%s</dc:contributor>"
				" <dc:date>%s</dc:date>"
				" <dc:description>%s</dc:description>"
				" <dc:publisher>%s</dc:publisher>"
				" <dc:language>%s</dc:language>"
				" <dc:relation>%s</dc:relation>"
				" <dc:rights>%s</dc:rights>"
				" <upnp:class>%s</upnp:class>"
				" <upnp:longdescription>%s</upnp:longdescription>"
				" <upnp:storagemedium>%s</upnp:storagemedium>"
				" <upnp:rating>%u</upnp:rating>"
				" <upnp:album>%s</upnp:album>"
				" <res protocolInfo=\"%s\" size=\"%llu\">http://%s:%d/upnp/contentdirectory?id=%s</res>"
				"</item>";
			rc = asprintf(&tmp, ifmt,
				id, pid, (entry->didl.restricted == 1) ? "true" : "false",
				title,
				entry->didl.dc.contributor,
				entry->didl.dc.date,
				entry->didl.dc.description,
				entry->didl.dc.publisher,
				entry->didl.dc.language,
				entry->didl.dc.relation,
				entry->didl.dc.rights,
				entry->didl.upnp.object.class,
				entry->didl.upnp.imageitem.longdescription,
				entry->didl.upnp.imageitem.rating,
				entry->didl.upnp.imageitem.storagemedium,
				entry->didl.upnp.photo.album,
				entry->didl.res.protocolinfo, entry->didl.res.size, upnp_getaddress(service->device->upnp), upnp_getport(service->device->upnp), path);
		} else {
			debugf("unknown class '%s'", entry->didl.upnp.object.class);
			free(out);
			free(id);
			free(pid);
			free(path);
			free(title);
			return NULL;
		}
		free(id);
		free(pid);
		free(path);
		free(title);
		if (rc < 0) {
			debugf("rc < 0");
			free(out);
			return NULL;
		}
		ptr = out;
		rc = asprintf(&out, "%s%s", ptr, tmp);
		free(ptr);
		free(tmp);
		if (rc < 0) {
			debugf("rc < 0");
			return NULL;
		}
		if (metadata == 1) {
			break;
		} else {
			entry = entry->next;
		}
	}
	ptr = out;
	rc = asprintf(&out, "%s%s", ptr, didk);
	free(ptr);
	if (rc < 0) {
		debugf("asprintf(); failed");
		return NULL;
	}
	return out;
}
