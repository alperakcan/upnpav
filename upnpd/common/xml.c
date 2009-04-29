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
#include <inttypes.h>
#include <time.h>
#include <pthread.h>

#include "upnp.h"
#include "common.h"

static void xmlescape_real (const char *str, char *target, int * length, int attribute)
{
	if (target != NULL) {
		int len = 0;

		for (; *str; str++) {
			if (*str == '<') {
				memcpy(target + len, "&lt;", 4);
				len += 4;
			} else if (attribute && (*str == '"')) {
				memcpy(target + len, "%22", 3);
				len += 3;
			} else if (*str == '>') {
				memcpy(target + len, "&gt;", 4);
				len += 4;
			} else if (*str == '&') {
				memcpy(target + len, "&amp;", 5);
				len += 5;
			} else {
				target[len++] = *str;
			}
		}
		target[len] = '\0';

		if (length != NULL)
			*length = len;
	} else if (length != NULL) {
		int len = 0;

		for (; *str; str++) {
			if (*str == '<') {
				len += 4;
			} else if (attribute && (*str == '"')) {
				len += 3;
			} else if (*str == '>') {
				len += 4;
			} else if (*str == '&') {
				len += 5;
			} else {
				len++;
			}
		}

		*length = len;
	}
}

char * xml_escape (const char *str, int attribute)
{
	int len;
	char *out;

	if (str == NULL) {
		return NULL;
	}
	xmlescape_real(str, NULL, &len, attribute);
	out = malloc(len + 1);
	xmlescape_real(str, out, NULL, attribute);
	return out;
}

char * xml_get_first_document_item (IXML_Document *doc, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	const char *val;
	char *ret = NULL;

	nodeList = ixmlDocument_getElementsByTagName(doc, (char *) item);
	if (nodeList == NULL) {
		return NULL;
	}
	if ((tmpNode = ixmlNodeList_item(nodeList, 0)) == NULL) {
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	textNode = ixmlNode_getFirstChild(tmpNode);
	val = ixmlNode_getNodeValue(textNode);
	if (val != NULL) {
		ret = strdup(val);
	}
	ixmlNodeList_free(nodeList);

	return ret;
}

char * xml_get_first_element_item (IXML_Element *element, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	const char *val;
	char *ret = NULL;

	nodeList = ixmlElement_getElementsByTagName(element, (char *) item);
	if (nodeList == NULL) {
		return NULL;
	}
	if ((tmpNode = ixmlNodeList_item(nodeList, 0)) == NULL) {
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	textNode = ixmlNode_getFirstChild(tmpNode);
	val = ixmlNode_getNodeValue(textNode);
	if (val != NULL) {
		ret = strdup(val);
	}
	ixmlNodeList_free(nodeList);

	return ret;
}

IXML_NodeList * xml_get_first_service_list (IXML_Document *doc)
{
	IXML_NodeList *ServiceList = NULL;
	IXML_NodeList *servlistnodelist = NULL;
	IXML_Node *servlistnode = NULL;
	servlistnodelist = ixmlDocument_getElementsByTagName(doc, "serviceList");
	if (servlistnodelist && ixmlNodeList_length( servlistnodelist)) {
		/* we only care about the first service list, from the root device
		 */
		servlistnode = ixmlNodeList_item(servlistnodelist, 0);
		/* create as list of DOM nodes
		 */
		ServiceList = ixmlElement_getElementsByTagName((IXML_Element *) servlistnode, "service");
	}
	if (servlistnodelist) {
		ixmlNodeList_free(servlistnodelist);
	}
	return ServiceList;
}

char * xml_get_element_value (IXML_Element *element)
{
	char *temp = NULL;
	IXML_Node *child = ixmlNode_getFirstChild((IXML_Node *) element);
	if ((child != 0) && (ixmlNode_getNodeType(child) == eTEXT_NODE)) {
    		temp = strdup(ixmlNode_getNodeValue(child));
    	}
    	return temp;
}

char * xml_get_string (IXML_Document *doc, const char *item)
{
	char *value;
	value = xml_get_first_document_item(doc, item);
	if (value) {
		return value;
	}
	return NULL;
}

uint32_t xml_get_ui4 (IXML_Document *doc, const char *item)
{
	uint32_t r;
	char *value;
	value = xml_get_first_document_item(doc, item);
	r = strtouint32(value);
	free(value);
	return r;
}