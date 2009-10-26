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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "platform.h"
#include "parser.h"

#include "expat.h"

typedef struct xml_data_s {
	char *path;
	char *name;
	char *value;
	char **attrs;
	unsigned int started;
	int (*callback) (void *context, const char *path, const char *name, const char **atrr, const char *value);
	void *context;
} xml_data_t;

static void xml_parse_start (void *xdata, const char *el, const char **xattr);
static void xml_parse_end (void *xdata, const char *el);
static void xml_parse_character (void *xdata, const char *txt, int txtlen);
static void xml_parse_start (void *xdata, const char *el, const char **xattr)
{
	int p;
	int l;
	char *tmp;
	xml_data_t *data;
	data = (xml_data_t *) xdata;
	if (data->callback != NULL) {
		if (data->started) {
			debugf("callback;");
			debugf("  path : '%s'", data->path);
			debugf("  name : '%s'", data->name);
			data->callback(data->context, data->path, data->name, (const char **) data->attrs, NULL);
		}
	}
	if (data->path == NULL) {
		data->path = strdup("");
	}
	l = strlen(data->path) + strlen("/") + strlen(el);
	tmp = (char *) malloc(l + 1);
	if (tmp == NULL) {
		return;
	}
	sprintf(tmp, "%s/%s", data->path, el);
	free(data->path);
	data->path = tmp;
	tmp = strrchr(data->path, '/');
	data->name = tmp + 1;
	free(data->value);
	data->value = NULL;
	for (p = 0; data->attrs && data->attrs[p] && data->attrs[p + 1]; p += 2) {
		free(data->attrs[p]);
		free(data->attrs[p + 1]);
	}
	free(data->attrs);
	for (p = 0; xattr[p] && xattr[p + 1]; p += 2) {
	}
	data->attrs = (char **) malloc(sizeof(char *) * (p + 2));
	if (data->attrs != NULL) {
		for (p = 0; xattr[p] && xattr[p + 1]; p += 2) {
			data->attrs[p] = strdup(xattr[p]);
			data->attrs[p + 1] = strdup(xattr[p + 1]);
		}
		data->attrs[p] = NULL;
		data->attrs[p + 1] = NULL;
	}
	data->started = 1;
}

static void xml_parse_end (void *xdata, const char *el)
{
	int p;
	char *ptr;
	xml_data_t *data;
	data = (xml_data_t *) xdata;
	if (data->callback != NULL) {
		if (data->name != NULL) {
			debugf("callback;");
			debugf("  path : '%s'", data->path);
			debugf("  name : '%s'", data->name);
			debugf("  value: '%s'", data->value);
			data->callback(data->context, data->path, data->name, (const char **) data->attrs, data->value);
		}
	}
	ptr = strrchr(data->path, '/');
	if (ptr != NULL) {
		*ptr = '\0';
	}
	if (strlen(data->path) == 0) {
		free(data->path);
		data->path = NULL;
	}
	data->started = 0;
	for (p = 0; data->attrs && data->attrs[p] && data->attrs[p + 1]; p += 2) {
		free(data->attrs[p]);
		free(data->attrs[p + 1]);
	}
	free(data->attrs);
	data->attrs = NULL;
	data->name = NULL;
	free(data->value);
	data->value = NULL;
}

static void xml_parse_character (void *xdata, const char *txt, int txtlen)
{
	xml_data_t *data;
	unsigned int total = 0;
	unsigned int total_old = 0;
	data = (xml_data_t *) xdata;
	if (txtlen <= 0 || txt == NULL || data->name == NULL) {
		return;
	}
	if (data->value != NULL) {
		total_old = strlen(data->value);
	}
	total = (total_old + txtlen + 1) * sizeof(char);
	data->value = (char *) realloc(data->value, total);
	if (total_old == 0) {
		data->value[0] = '\0';
	}
	strncat(data->value, txt, txtlen);
}

int xml_parse_buffer_callback (const char *buffer, unsigned int len, int (*callback) (void *context, const char *path, const char *name, const char **atrr, const char *value), void *context)
{
	XML_Parser p;
	xml_data_t *data;
	data = (xml_data_t *) malloc(sizeof(xml_data_t));
	if (data == NULL) {
		return -1;
	}
	memset(data, 0, sizeof(xml_data_t));
	data->callback = callback;
	data->context = context;
	p = XML_ParserCreate(NULL);
	XML_SetUserData(p, data);
	XML_SetElementHandler(p, xml_parse_start, xml_parse_end);
	XML_SetCharacterDataHandler(p, xml_parse_character);
	if (!XML_Parse(p, buffer, len, 1)) {
		debugf("Parse error at line %d:%s", (int) XML_GetCurrentLineNumber(p), XML_ErrorString(XML_GetErrorCode(p)));
		XML_ParserFree(p);
		free(data->path);
		free(data);
		return -1;
	}
	XML_ParserFree(p);
	free(data->path);
	free(data);
	return 0;
}
