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
#include <fcntl.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include "common.h"

/**
  */
typedef struct contentdir_s {
	/** */
	device_service_t service;
	/** */
	uint32_t updateid;
	/** */
	entry_t *root;
	/** */
	char *rootpath;
	/** */
	int cached;
} contentdir_t;

static int contentdirectory_get_search_capabilities (device_service_t *service, upnp_event_action_t *request)
{
	int rc;
	debugf("contentdir get search capabilities");
	rc = upnp_add_response(request, service->type, "SearchCaps", "*");
	return rc;
}

static int contentdirectory_get_sort_capabilities (device_service_t *service, upnp_event_action_t *request)
{
	int rc;
	debugf("contentdirectory get sort capabilities");
	rc = upnp_add_response(request, service->type, "SortCaps", "*");
	return rc;
}

static int contentdirectory_get_system_update_id (device_service_t *service, upnp_event_action_t *request)
{
	int rc;
	char str[23];
	debugf("contentdirectory get system update id");
	rc = upnp_add_response(request, service->type, "Id", uint32tostr(str, ((contentdir_t *) service)->updateid));
	return rc;
}

static int contentdirectory_browse (device_service_t *service, upnp_event_action_t *request)
{
	char str[23];
	entry_t *entry;
	contentdir_t *contentdir;

	/* in */
	char *objectid;
	char *browseflag;
	char *filter;
	uint32_t startingindex;
	uint32_t requestedcount;
	char *sortcriteria;
	/* out */
	char *result;
	uint32_t totalmatches;
	uint32_t numberreturned;
	uint32_t updateid;

	contentdir = (contentdir_t *) service;

	objectid = xml_get_string(request->request, "ObjectID");
	browseflag = xml_get_string(request->request, "BrowseFlag");
	filter = xml_get_string(request->request, "Filter");
	startingindex = xml_get_ui4(request->request, "StartingIndex");
	requestedcount = xml_get_ui4(request->request, "RequestedCount");
	sortcriteria = xml_get_string(request->request, "SortCriteria");

	debugf("contentdirectory browse:\n"
		"  objectid      : %s\n"
		"  browseflag    : %s\n"
		"  filter        : %s\n"
		"  startingindex : %u\n"
		"  requestedcount: %u\n"
		"  sortcriteria  : %s\n",
		objectid,
		browseflag,
		filter,
		startingindex,
		requestedcount,
		sortcriteria);

	if (strcmp(browseflag, "BrowseMetadata") == 0) {
		if (contentdir->cached) {
			entry = entry_id(contentdir->root, (objectid != NULL) ? objectid : "0");
		} else {
			char *id;
			if (objectid == NULL || strcmp(objectid, "0") == 0) {
				entry = entry_didl(0, contentdir->rootpath);
				entry_normalize_root(entry);
			} else {
				id = entryid_path_from_id(objectid);
				entry = entry_didl(0, id);
				free(id);
			}
			id = entryid_id_from_path(contentdir->rootpath);
			if (strcmp(entry->didl.parentid, id) == 0) {
				entry_normalize_parent(entry);
			}
			free(id);
		}
		if (entry == NULL) {
			request->errcode = 701;
			free(objectid);
			free(browseflag);
			free(filter);
			free(sortcriteria);
			if (contentdir->cached == 0) {
				entry_uninit(entry);
			}
			return -1;
		}
		totalmatches = 1;
		numberreturned = 1;
		updateid = contentdir->updateid;
		result = entry_to_result(service, entry, 1, startingindex, requestedcount, &numberreturned);
		if (result == NULL) {
			request->errcode = 720;
			free(objectid);
			free(browseflag);
			free(filter);
			free(sortcriteria);
			if (contentdir->cached == 0) {
				entry_uninit(entry);
			}
			return -1;
		}
		upnp_add_response(request, service->type, "Result", result);
		upnp_add_response(request, service->type, "NumberReturned", uint32tostr(str, numberreturned));
		upnp_add_response(request, service->type, "TotalMatches", uint32tostr(str, totalmatches));
		upnp_add_response(request, service->type, "UpdateID", uint32tostr(str, updateid));
		free(result);
		free(objectid);
		free(browseflag);
		free(filter);
		free(sortcriteria);
		if (contentdir->cached == 0) {
			entry_uninit(entry);
		}
		return 0;
	} else if (strcmp(browseflag, "BrowseDirectChildren") == 0) {
		if (contentdir->cached) {
			entry = entry_id(contentdir->root, (objectid != NULL) ? objectid : "0");
		} else {
			char *id;
			if (objectid == NULL || strcmp(objectid, "0") == 0) {
				entry = entry_init(contentdir->rootpath, 0);
				entry_normalize_root(entry);
			} else {
				id = entryid_path_from_id(objectid);
				entry = entry_init(id, 0);
				free(id);
			}
		}
		if (entry == NULL) {
			request->errcode = 701;
			free(objectid);
			free(browseflag);
			free(filter);
			free(sortcriteria);
			return -1;
		}
#if 0
		if (entry->didl.childcount == 0) {
			request->ActionResult = NULL;
			request->ErrCode = 720;
			strcpy(request->ErrStr, "object is not a container");
			free(objectid);
			free(browseflag);
			free(filter);
			free(sortcriteria);
			if (contentdir->cached == 0) {
				entry_uninit(entry);
			}
			return -1;
		}
#endif
		totalmatches = entry->didl.childcount;
		updateid = contentdir->updateid;
		result = entry_to_result(service, entry, 0, startingindex, requestedcount, &numberreturned);
		if (result == NULL) {
			request->errcode = 720;
			free(objectid);
			free(browseflag);
			free(filter);
			free(sortcriteria);
			if (contentdir->cached == 0) {
				entry_uninit(entry);
			}
			return -1;
		}
		upnp_add_response(request, service->type, "Result", result);
		upnp_add_response(request, service->type, "NumberReturned", uint32tostr(str, numberreturned));
		upnp_add_response(request, service->type, "TotalMatches", uint32tostr(str, totalmatches));
		upnp_add_response(request, service->type, "UpdateID", uint32tostr(str, updateid));
		free(result);
		free(objectid);
		free(browseflag);
		free(filter);
		free(sortcriteria);
		if (contentdir->cached == 0) {
			entry_uninit(entry);
		}
		return 0;
	} else {
		request->errcode = 402;
		free(objectid);
		free(browseflag);
		free(filter);
		free(sortcriteria);
		return -1;
	}
}

static char *allowed_values_browseflag[] = {
	"BrowseMetadata",
	"BrowseDirectChildren",
	NULL,
};

static service_variable_t *contentdirectory_variables[] = {
	/* required */
	& (service_variable_t) {"A_ARG_TYPE_ObjectID", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Result", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_BrowseFlag", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, allowed_values_browseflag, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Filter", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_SortCriteria", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Index", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_Count", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"A_ARG_TYPE_UpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"SearchCapabilities", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"SortCapabilities", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
	& (service_variable_t) {"SystemUpdateID", VARIABLE_DATATYPE_UI4, VARIABLE_SENDEVENT_YES, NULL, NULL},
	NULL,
};

static action_argument_t *arguments_get_search_capabilities[] = {
	& (action_argument_t) {"SearchCaps", ARGUMENT_DIRECTION_OUT, "SearchCapabilities"},
	NULL,
};

static action_argument_t *arguments_get_sort_capabilities[] = {
	& (action_argument_t) {"SortCaps", ARGUMENT_DIRECTION_OUT, "SortCapabilities"},
	NULL,
};

static action_argument_t *arguments_get_system_update_id[] = {
	& (action_argument_t) {"Id", ARGUMENT_DIRECTION_OUT, "SystemUpdateID"},
	NULL,
};

static action_argument_t *arguments_browse[] = {
	& (action_argument_t) {"ObjectID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ObjectID"},
	& (action_argument_t) {"BrowseFlag", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_BrowseFlag"},
	& (action_argument_t) {"Filter", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_Filter"},
	& (action_argument_t) {"StartingIndex", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_Index"},
	& (action_argument_t) {"RequestedCount", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_Count"},
	& (action_argument_t) {"SortCriteria", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_SortCriteria"},
	& (action_argument_t) {"Result", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_Result"},
	& (action_argument_t) {"NumberReturned", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_Count"},
	& (action_argument_t) {"TotalMatches", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_Count"},
	& (action_argument_t) {"UpdateID", ARGUMENT_DIRECTION_OUT, "A_ARG_TYPE_UpdateID"},
	NULL,
};

static service_action_t *contentdirectory_actions[] = {
	/* required */
	& (service_action_t) {"GetSearchCapabilities", arguments_get_search_capabilities, contentdirectory_get_search_capabilities},
	& (service_action_t) {"GetSortCapabilities", arguments_get_sort_capabilities, contentdirectory_get_sort_capabilities},
	& (service_action_t) {"GetSystemUpdateID", arguments_get_system_update_id, contentdirectory_get_system_update_id},
	& (service_action_t) {"Browse", arguments_browse, contentdirectory_browse},
	NULL,
};

static int contentdirectory_vfsgetinfo (void *cookie, char *path, webserver_fileinfo_t *info)
{
	char *id;
	entry_t *entry;
	struct stat stbuf;
	const char *ename;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsgetinfo '%s'", path);
	contentdir = (contentdir_t *) cookie;
	if (strncmp(path, "/upnp/contentdirectory?id=", strlen("/upnp/contentdirectory?id=")) != 0) {
		debugf("file is not belong to content directory");
		return -1;
	}
	ename = path + strlen("/upnp/contentdirectory?id=");
	debugf("entry name is '%s'", ename);
	if (contentdir->cached) {
		entry = entry_id(contentdir->root, (char *) ename);
	} else {
		id = entryid_path_from_id((char *) ename);
		entry = entry_didl(0, id);
		free(id);
	}
	if (entry == NULL) {
		debugf("no entry found '%s'", ename);
		return -1;
	}
	debugf("entry path is '%s'", entry->path);
	if (access(entry->path, R_OK) == 0 && stat(entry->path, &stbuf) == 0) {
		info->size = stbuf.st_size;
		info->mtime = stbuf.st_mtime;
		info->mimetype = strdup(entry->mime);
		if (contentdir->cached == 0) {
			entry_uninit(entry);
		}
		return 0;
	}
	if (contentdir->cached == 0) {
		entry_uninit(entry);
	}
	debugf("no file found");
	return -1;
}

static void * contentdirectory_vfsopen (void *cookie, char *path, webserver_filemode_t mode)
{
	char *id;
	file_t *file;
	entry_t *entry;
	const char *ename;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsopen");
	contentdir = (contentdir_t *) cookie;
	if (strncmp(path, "/upnp/contentdirectory?id=", strlen("/upnp/contentdirectory?id=")) != 0) {
		debugf("file is not belong to content directory");
		return NULL;
	}
	ename = path + strlen("/upnp/contentdirectory?id=");
	debugf("entry name is '%s'", ename);
	if (contentdir->cached) {
		entry = entry_id(contentdir->root, (char *) ename);
	} else {
		id = entryid_path_from_id((char *) ename);
		entry = entry_didl(0, id);
		free(id);
	}
	if (entry == NULL) {
		debugf("no entry found '%s'", ename);
		return NULL;
	}
	debugf("entry path is '%s'", entry->path);
	file = (file_t *) malloc(sizeof(file_t));
	if (file == NULL) {
		debugf("malloc failed");
		if (contentdir->cached == 0) {
			entry_uninit(entry);
		}
		return NULL;
	}
	memset(file, 0, sizeof(file_t));
	file->virtual = 0;
	file->fd = open(entry->path, O_RDONLY);
	file->service = &contentdir->service;
	if (file->fd < 0) {
		debugf("open(%s, O_RDONLY); failed", ename);
		free(file);
		if (contentdir->cached == 0) {
			entry_uninit(entry);
		}
		return NULL;
	}
	if (contentdir->cached == 0) {
		entry_uninit(entry);
	}
	return file;
}

static int contentdirectory_vfsread (void *cookie, void *handle, char *buffer, unsigned int length)
{
	file_t *file;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsread");
	file = (file_t *) handle;
	contentdir = (contentdir_t *) cookie;
	return read(file->fd, buffer, length);
}

static int contentdirectory_vfswrite (void *cookie, void *handle, char *buffer, unsigned int length)
{
	contentdir_t *contentdir;
	debugf("contentdirectory_vfswrite");
	contentdir = (contentdir_t *) cookie;
	return -1;
}

static unsigned long contentdirectory_vfsseek (void *cookie, void *handle, long offset, webserver_seek_t whence)
{
	file_t *file;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsseek");
	file = (file_t *) handle;
	contentdir = (contentdir_t *) cookie;
	switch (whence) {
		case WEBSERVER_SEEK_SET: return lseek(file->fd, offset, SEEK_SET);
		case WEBSERVER_SEEK_CUR: return lseek(file->fd, offset, SEEK_CUR);
		case WEBSERVER_SEEK_END: return lseek(file->fd, offset, SEEK_END);
	}
	return -1;
}

static int contentdirectory_vfsclose (void *cookie, void *handle)
{
	file_t *file;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsclose");
	file = (file_t *) handle;
	contentdir = (contentdir_t *) cookie;
	close(file->fd);
	free(file);
	return 0;
}

static webserver_callbacks_t contentdir_vfscallbacks = {
	contentdirectory_vfsgetinfo,
	contentdirectory_vfsopen,
	contentdirectory_vfsread,
	contentdirectory_vfswrite,
	contentdirectory_vfsseek,
	contentdirectory_vfsclose,
};

int contentdirectory_uninit (device_service_t *contentdir)
{
	int i;
	service_variable_t *variable;
	debugf("contentdirectory uninit");
	debugf("uninitializing entry database");
	entry_uninit(((contentdir_t *) contentdir)->root);
	free(((contentdir_t *) contentdir)->rootpath);
	for (i = 0; (variable = contentdir->variables[i]) != NULL; i++) {
		free(variable->value);
	}
	free(contentdir);
	return 0;
}

device_service_t * contentdirectory_init (char *directory, int cached)
{
	contentdir_t *contentdir;
	service_variable_t *variable;
	contentdir = NULL;
	if (directory == NULL) {
		debugf("please specify a database directory for contentdirectory service");
		goto out;
	}
	debugf("initializing content directory service struct");
	contentdir = (contentdir_t *) malloc(sizeof(contentdir_t));
	if (contentdir == NULL) {
		debugf("malloc(sizeof(contentdir_t)) failed");
		goto out;
	}
	memset(contentdir, 0, sizeof(contentdir_t));
	contentdir->service.name = "contentdirectory";
	contentdir->service.id = "urn:upnp-org:serviceId:ContentDirectory";
	contentdir->service.type = "urn:schemas-upnp-org:service:ContentDirectory:1";
	contentdir->service.scpdurl = "/upnp/contentdirectory.xml";
	contentdir->service.eventurl = "/upnp/event/contendirectory1";
	contentdir->service.controlurl = "/upnp/control/contentdirectory1";
	contentdir->service.actions = contentdirectory_actions;
	contentdir->service.variables = contentdirectory_variables;
	contentdir->service.vfscallbacks = &contentdir_vfscallbacks;
	contentdir->service.uninit = contentdirectory_uninit;
	contentdir->updateid = 0;
	contentdir->cached = cached;

	debugf("initializing content directory service");
	if (service_init(&contentdir->service) != 0) {
		debugf("service_init(&contentdir->service) failed");
		free(contentdir);
		contentdir = NULL;
		goto out;
	}
	variable = service_variable_find(&contentdir->service, "SystemUpdateID");
	if (variable != NULL) {
		variable->value = strdup("0");
	}
	debugf("initializing entry database");
	contentdir->rootpath = strdup(directory);
	if (contentdir->cached) {
		contentdir->root = entry_init(directory, 1);
		entry_normalize_root(contentdir->root);
	}

	debugf("initialized content directory service");
out:	return &contentdir->service;
}
