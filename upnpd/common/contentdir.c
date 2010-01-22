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

#include "platform.h"
#include "database.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

/**
  */
typedef struct contentdir_s {
	/** */
	device_service_t service;
	/** */
	uint32_t updateid;
	/** */
	database_t *database;
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

typedef struct contentdirectory_browse_data_s {
	char *objectid;
	char *browseflag;
	char *filter;
	uint32_t startingindex;
	uint32_t requestedcount;
	char *sortcriteria;
} contentdirectory_browse_data_t;

static int contentdirectory_browse_callback (void *context, const char *path, const char *name, const char **atrr, const char *value)
{
	contentdirectory_browse_data_t *data;
	data = (contentdirectory_browse_data_t *) context;
	if (strcmp(path, "/s:Envelope/s:Body/u:Browse/ObjectID") == 0 ||
	    strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Browse/ObjectID") == 0) {
		data->objectid = (value) ? strdup(value) : NULL;
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Browse/BrowseFlag") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Browse/BrowseFlag") == 0) {
		data->browseflag = (value) ? strdup(value) : NULL;
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Browse/Filter") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Browse/Filter") == 0) {
		data->filter = (value) ? strdup(value) : NULL;
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Browse/StartingIndex") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Browse/StartingIndex") == 0) {
		data->startingindex = strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Browse/RequestedCount") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Browse/RequestedCount") == 0) {
		data->requestedcount = strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Browse/SortCriteria") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Browse/SortCriteria") == 0) {
		data->sortcriteria = (value) ? strdup(value) : NULL;
	}
	return 0;
}

static int contentdirectory_browse (device_service_t *service, upnp_event_action_t *request)
{
	char *id;
	char str[23];
	entry_t *tmp;
	entry_t *entry;
	contentdir_t *contentdir;
	contentdirectory_browse_data_t data;

	/* out */
	char *result;
	uint32_t totalmatches;
	uint32_t numberreturned;
	uint32_t updateid;

	contentdir = (contentdir_t *) service;

	entry = NULL;
	totalmatches = 0;
	numberreturned = 0;
	updateid = contentdir->updateid;

	memset(&data, 0, sizeof(data));
	if (xml_parse_buffer_callback(request->request, strlen(request->request), contentdirectory_browse_callback, &data) != 0) {
		debugf("xml_parse_buffer_callback() failed");
		request->errcode = UPNP_ERROR_PARAMETER_MISMATCH;
		return 0;
	}
	debugf("contentdirectory browse:\n"
		"  objectid      : %s\n"
		"  browseflag    : %s\n"
		"  filter        : %s\n"
		"  startingindex : %u\n"
		"  requestedcount: %u\n"
		"  sortcriteria  : %s\n",
		data.objectid,
		data.browseflag,
		data.filter,
		data.startingindex,
		data.requestedcount,
		data.sortcriteria);

	if (strcmp(data.browseflag, "BrowseMetadata") == 0) {
		if (data.objectid == NULL || strcmp(data.objectid, "0") == 0) {
			entry = entry_didl_from_path(contentdir->rootpath);
			debugf("found entry %p", entry);
			tmp = entry;
			while (tmp != NULL) {
				free(entry->didl.entryid);
				free(entry->didl.parentid);
				entry->didl.entryid = strdup("0");
				entry->didl.parentid = strdup("-1");
				if (entry->didl.entryid == NULL ||
				    entry->didl.parentid == NULL) {
					debugf("strdup('0') failed");
					request->errcode = UPNP_ERROR_CANNOT_PROCESS;
					goto error;
				}
				tmp = tmp->next;
			}
		} else {
			debugf("looking for '%s'", data.objectid);
			entry = entry_didl_from_id(contentdir->database, data.objectid);
		}
		if (entry != NULL) {
			if (contentdir->cached == 0) {
				id = entryid_id_from_path(contentdir->rootpath);
				if (strcmp(entry->didl.parentid, id) == 0) {
					free(entry->didl.parentid);
					entry->didl.parentid = strdup("0");
					if (entry->didl.parentid == NULL) {
						debugf("strdup('0') failed");
						request->errcode = UPNP_ERROR_CANNOT_PROCESS;
						goto error;				}
				}
				free(id);
			}
		} else {
			debugf("could not find object '%s'", data.objectid);
		}
		if (entry == NULL) {
			request->errcode = UPNP_ERROR_NOSUCH_OBJECT;
			goto error;
		}
		totalmatches = 1;
		numberreturned = 1;
		updateid = contentdir->updateid;
		result = entry_to_result(service, entry, 1);
		if (result == NULL) {
			request->errcode = UPNP_ERROR_CANNOT_PROCESS;
			goto error;
		}
		numberreturned = 1;
		totalmatches = 1;
		upnp_add_response(request, service->type, "Result", result);
		upnp_add_response(request, service->type, "NumberReturned", uint32tostr(str, numberreturned));
		upnp_add_response(request, service->type, "TotalMatches", uint32tostr(str, totalmatches));
		upnp_add_response(request, service->type, "UpdateID", uint32tostr(str, updateid));
		free(data.browseflag);
		free(data.filter);
		free(data.objectid);
		free(data.sortcriteria);
		free(result);
		entry_uninit(entry);
		return 0;
	} else if (strcmp(data.browseflag, "BrowseDirectChildren") == 0) {
		if (contentdir->cached == 0 && (data.objectid == NULL || strcmp(data.objectid, "0") == 0)) {
			entry = entry_init_from_path(contentdir->rootpath, data.startingindex, data.requestedcount, &numberreturned, &totalmatches);
			tmp = entry;
			while (tmp != NULL) {
				free(tmp->didl.parentid);
				tmp->didl.parentid = strdup("0");
				if (entry->didl.parentid == NULL) {
					debugf("strdup('0') failed");
					request->errcode = UPNP_ERROR_CANNOT_PROCESS;
					goto error;
				}
				tmp = tmp->next;
			}
		} else {
			debugf("looking for '%s'", data.objectid);
			entry = entry_init_from_id(contentdir->database, data.objectid, data.startingindex, data.requestedcount, &numberreturned, &totalmatches);
		}
		if (entry == NULL) {
			debugf("could not find any child object");
			entry = entry_didl_from_id(contentdir->database, data.objectid);
			if (entry == NULL) {
				debugf("could not find any object");
				request->errcode = UPNP_ERROR_NOSUCH_OBJECT;
				goto error;
			}
			entry_uninit(entry);
			entry = NULL;
		}
		updateid = contentdir->updateid;
		result = entry_to_result(service, entry, 0);
		if (result == NULL) {
			request->errcode = UPNP_ERROR_CANNOT_PROCESS;
			goto error;
		}
		upnp_add_response(request, service->type, "Result", result);
		upnp_add_response(request, service->type, "NumberReturned", uint32tostr(str, numberreturned));
		upnp_add_response(request, service->type, "TotalMatches", uint32tostr(str, totalmatches));
		upnp_add_response(request, service->type, "UpdateID", uint32tostr(str, updateid));
		free(data.browseflag);
		free(data.filter);
		free(data.objectid);
		free(data.sortcriteria);
		free(result);
		entry_uninit(entry);
		return 0;
	} else {
		request->errcode = UPNP_ERROR_INVALIG_ARGS;
error:
		free(data.browseflag);
		free(data.filter);
		free(data.objectid);
		free(data.sortcriteria);
		entry_uninit(entry);
		return -1;
	}
}

typedef struct contentdirectory_search_data_s {
	char *objectid;
	char *searchflag;
	char *filter;
	uint32_t startingindex;
	uint32_t requestedcount;
	char *sortcriteria;
} contentdirectory_search_data_t;

static int contentdirectory_search_callback (void *context, const char *path, const char *name, const char **atrr, const char *value)
{
	contentdirectory_search_data_t *data;
	data = (contentdirectory_search_data_t *) context;
	if (strcmp(path, "/s:Envelope/s:Body/u:Search/ContainerID") == 0 ||
	    strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Search/ContainerID") == 0) {
		data->objectid = (value) ? strdup(value) : NULL;
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Browse/SearchCriteria") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Search/SearchCriteria") == 0) {
		data->searchflag = (value) ? strdup(value) : NULL;
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Search/Filter") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Search/Filter") == 0) {
		data->filter = (value) ? strdup(value) : NULL;
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Search/StartingIndex") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Search/StartingIndex") == 0) {
		data->startingindex = strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Search/RequestedCount") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Search/RequestedCount") == 0) {
		data->requestedcount = strtouint32(value);
	} else if (strcmp(path, "/s:Envelope/s:Body/u:Search/SortCriteria") == 0 ||
		   strcmp(path, "/SOAP-ENV:Envelope/SOAP-ENV:Body/m:Search/SortCriteria") == 0) {
		data->sortcriteria = (value) ? strdup(value) : NULL;
	}
	return 0;
}

static int contentdirectory_search (device_service_t *service, upnp_event_action_t *request)
{
	contentdir_t *contentdir;
	contentdirectory_search_data_t data;

	char str[23];
	entry_t *entry;

	/* out */
	char *result;
	uint32_t totalmatches;
	uint32_t numberreturned;
	uint32_t updateid;

	contentdir = (contentdir_t *) service;

	memset(&data, 0, sizeof(data));
	if (xml_parse_buffer_callback(request->request, strlen(request->request), contentdirectory_search_callback, &data) != 0) {
		debugf("xml_parse_buffer_callback() failed");
		request->errcode = UPNP_ERROR_PARAMETER_MISMATCH;
		return 0;
	}
	debugf("contentdirectory search:\n"
		"  objectid      : %s\n"
		"  searchflag    : %s\n"
		"  filter        : %s\n"
		"  startingindex : %u\n"
		"  requestedcount: %u\n"
		"  sortcriteria  : %s\n",
		data.objectid,
		data.searchflag,
		data.filter,
		data.startingindex,
		data.requestedcount,
		data.sortcriteria);

	entry = entry_init_from_search(contentdir->database, data.objectid, data.startingindex, data.requestedcount, &numberreturned, &totalmatches, data.searchflag);
	if (entry == NULL) {
		debugf("could not find any object");
		request->errcode = UPNP_ERROR_NOSUCH_OBJECT;
		goto error;
	}
	updateid = contentdir->updateid;
	result = entry_to_result(service, entry, 0);
	if (result == NULL) {
		request->errcode = UPNP_ERROR_CANNOT_PROCESS;
		goto error;
	}
	upnp_add_response(request, service->type, "Result", result);
	upnp_add_response(request, service->type, "NumberReturned", uint32tostr(str, numberreturned));
	upnp_add_response(request, service->type, "TotalMatches", uint32tostr(str, totalmatches));
	upnp_add_response(request, service->type, "UpdateID", uint32tostr(str, updateid));
	free(data.searchflag);
	free(data.filter);
	free(data.objectid);
	free(data.sortcriteria);
	free(result);
	entry_uninit(entry);
	return 0;

	request->errcode = UPNP_ERROR_INVALIG_ARGS;
error:
	free(data.searchflag);
	free(data.filter);
	free(data.objectid);
	free(data.sortcriteria);
	entry_uninit(entry);
	return -1;
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
	& (service_variable_t) {"A_ARG_TYPE_SearchCriteria", VARIABLE_DATATYPE_STRING, VARIABLE_SENDEVENT_NO, NULL, NULL},
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

static action_argument_t *arguments_search[] = {
	& (action_argument_t) {"ContainerID", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_ObjectID"},
	& (action_argument_t) {"SearchCriteria", ARGUMENT_DIRECTION_IN, "A_ARG_TYPE_SearchCriteria"},
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
	& (service_action_t) {"Search", arguments_search, contentdirectory_search},
	NULL,
};

static int contentdirectory_istranscode (const char *title)
{
	if (strncmp(title, "[transcode] - ", strlen("[transcode] - ")) == 0) {
		return 0;
	}
	return -1;
}

static int contentdirectory_vfsgetinfo (void *cookie, char *path, gena_fileinfo_t *info)
{
	char *ptr;
	entry_t *entry;
	file_stat_t stat;
	const char *ename;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsgetinfo '%s'", path);
	contentdir = (contentdir_t *) cookie;
	if (strncmp(path, "/upnp/contentdirectory?id=", strlen("/upnp/contentdirectory?id=")) != 0) {
		debugf("file is not belong to content directory");
		return -1;
	}
	ename = path + strlen("/upnp/contentdirectory?id=");
	ptr = strstr(ename, "&");
	if (ptr != NULL) {
		*ptr = '\0';
	}
	debugf("entry name is '%s'", ename);
	entry = entry_didl_from_id(contentdir->database, ename);
	if (entry == NULL) {
		debugf("no entry found '%s'", ename);
		return -1;
	}
	debugf("entry path: '%s', title: '%s'", entry->path, entry->didl.dc.title);
	if (contentdirectory_istranscode(entry->didl.dc.title) == 0) {
		debugf("transcode not supportted yet");
		entry_uninit(entry);
		return -1;
	}
	if (file_access(entry->path, FILE_MODE_READ) == 0 &&
	    file_stat(entry->path, &stat) == 0) {
		info->size = entry->didl.res.size;
		info->mtime = stat.mtime;
		info->mimetype = strdup(entry->mime);
		entry_uninit(entry);
		return 0;
	}
	entry_uninit(entry);
	debugf("no file found");
	return -1;
}

static void * contentdirectory_vfsopen (void *cookie, char *path, gena_filemode_t mode)
{
	entry_t *entry;
	upnp_file_t *file;
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
	entry = entry_didl_from_id(contentdir->database, ename);
	if (entry == NULL) {
		debugf("no entry found '%s'", ename);
		return NULL;
	}
	debugf("entry path: '%s', title: '%s'", entry->path, entry->didl.dc.title);
	file = (upnp_file_t *) malloc(sizeof(upnp_file_t));
	if (file == NULL) {
		debugf("malloc failed");
		entry_uninit(entry);
		return NULL;
	}
	memset(file, 0, sizeof(upnp_file_t));
	file->virtual = 0;
	file->transcode = 0;
	file->file = file_open(entry->path, FILE_MODE_READ);
	file->service = &contentdir->service;
	if (file->file == NULL) {
		debugf("open(%s, O_RDONLY); failed", ename);
		free(file);
		entry_uninit(entry);
		return NULL;
	}
	entry_uninit(entry);
	return file;
}

static int contentdirectory_vfsread (void *cookie, void *handle, char *buffer, unsigned int length)
{
	upnp_file_t *file;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsread");
	file = (upnp_file_t *) handle;
	contentdir = (contentdir_t *) cookie;
	return file_read(file->file, buffer, length);
}

static int contentdirectory_vfswrite (void *cookie, void *handle, char *buffer, unsigned int length)
{
	contentdir_t *contentdir;
	debugf("contentdirectory_vfswrite");
	contentdir = (contentdir_t *) cookie;
	return -1;
}

static unsigned long long contentdirectory_vfsseek (void *cookie, void *handle, unsigned long long offset, gena_seek_t whence)
{
	upnp_file_t *file;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsseek");
	file = (upnp_file_t *) handle;
	contentdir = (contentdir_t *) cookie;
	switch (whence) {
		case GENA_SEEK_SET: return file_seek(file->file, offset, FILE_SEEK_SET);
		case GENA_SEEK_CUR: return file_seek(file->file, offset, FILE_SEEK_CUR);
		case GENA_SEEK_END: return file_seek(file->file, offset, FILE_SEEK_END);
	}
	return -1;
}

static int contentdirectory_vfsclose (void *cookie, void *handle)
{
	upnp_file_t *file;
	contentdir_t *contentdir;
	debugf("contentdirectory_vfsclose");
	file = (upnp_file_t *) handle;
	contentdir = (contentdir_t *) cookie;
	file_close(file->file);
	free(file);
	return 0;
}

static gena_callback_vfs_t contentdir_vfscallbacks = {
	contentdirectory_vfsgetinfo,
	contentdirectory_vfsopen,
	contentdirectory_vfsread,
	contentdirectory_vfswrite,
	contentdirectory_vfsseek,
	contentdirectory_vfsclose,
};

static int contentdirectory_uninit (device_service_t *contentdir)
{
	int i;
	service_variable_t *variable;
	debugf("contentdirectory uninit");
	debugf("uninitializing entry database");
	if ((database_t *) ((contentdir_t *) contentdir)->database != NULL) {
		database_uninit((database_t *) ((contentdir_t *) contentdir)->database, 0);
	}
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
	contentdir->service.eventurl = "/upnp/event/contentdirectory1";
	contentdir->service.controlurl = "/upnp/control/contentdirectory1";
	contentdir->service.actions = contentdirectory_actions;
	contentdir->service.variables = contentdirectory_variables;
	contentdir->service.vfscallbacks = &contentdir_vfscallbacks;
	contentdir->service.uninit = contentdirectory_uninit;
	contentdir->updateid = 0;

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
	contentdir->cached = cached;

	if (contentdir->cached) {
		contentdir->database = entry_scan(contentdir->rootpath, (cached == 1) ? 0 :  1);
	}

	debugf("initialized content directory service");
out:	return &contentdir->service;
}
