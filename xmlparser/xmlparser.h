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

/** @defgroup xmlparser XML Parser API
  * @brief XML parse and handling api.
  *
  * @example
  *
  * @code
  * <xml>
  * 	<a>
  * 		<b>b</b>
  * 		<c c0="c0" c1="c1">
  * 			<d>d</d>
  * 		</c>
  * 	</a>
  * 	<e>e</e>
  * </xml>
  *
  * {
  * 	xml_node_t *xml;
  * 	xml_node_t *xml_e;
  * 	xml_node_t *xml_d;
  * 	xml_node_t *xml_c;
  * 	xml_node_attr_t *xml_c_attr;
  *
  * 	xml_parse_file(&xml, &fle);
  *
  * 	xml_d = xml_node_get_path(xml, "xml/a/c/d");
  * 	printf("xml/a/c/d value: %s\n", xml_node_get_value(xml_d);
  * 	xml_e = xml_node_get_path(xml, "xml/e");
  * 	printf("xml/e value: %s\n", xml_node_get_value(xml_e);
  *
  * 	xml_c = xml_node_get_path(xml, "xml/a/c");
  * 	xml_c_attr = xml_node_get_attr(xml_c, "c0");
  * 	printf("xml/c attr c0 value: %s\n", xml_node_get_attr_value(xml_c, "c0"));
  *
  * 	xml_node_uninit(xml);
  * }
  * @endcode
  */

/** @addtogroup xmlparser */
/*@{*/

typedef struct xml_node_attr_s xml_node_attr_t;
typedef struct xml_node_s xml_node_t;

/** xml node attributes struct
  */
struct xml_node_attr_s {
	/** list head */
	list_t head;
	/** attr name */
	char *name_;
	/** attr value */
	char *value_;
};

/** xml node struct
  */
struct xml_node_s {
	/** list head */
	list_t head;
	/** node name */
	char *name_;
	/** node value */
	char *value_;
	/** node attributes */
	list_t attrs;
	/** child nodes */
	list_t nodes;
	/** parent node */
	xml_node_t *parent;
};

/** @brief return the node for given path, and head node
  *
  * @param *node - head node to start
  * @param *path - the requested path
  * @returns node for given path, or NULL if not found.
  */
xml_node_t * xml_node_get_path (xml_node_t *node, const char *path);

/** @brief returns the value for given node
  *
  * @param *node - the node
  * @returns name for given node, or NULL if no value.
  */
char * xml_node_get_name (xml_node_t *node);

/** @brief returns the value for given node
  *
  * @param *node - the node
  * @returns value for given node, or NULL if no value.
  */
char * xml_node_get_value (xml_node_t *node);

/** @brief return the node value for given path, and head node
  *
  * @param *node - head node to start
  * @param *path - the requested path
  * @returns value for given path, or NULL if not found.
  */
char * xml_node_get_path_value (xml_node_t *node, const char *path);

/** @brief returns the attribute "attr", for given node
  *
  * @param *node - the node
  * @param *attr - requested attribute
  * @return the attribute, or NULL if not found
  */
xml_node_attr_t * xml_node_get_attr (xml_node_t *node, const char *attr);

/** @brief returns the value for given node, and attribute
  *
  * @param *node - the noe
  * @param *attr - the attribute
  * @returns value for given node, or NULL if no value.
  */
char * xml_node_get_attr_value (xml_node_t *node, const char *attr);

/** @brief dublicates the given node
  *
  * @param *node - node to dublicate
  * @param **dub - dublicated value
  * @returns 0 on success
  */
int xml_node_dublicate (xml_node_t *node, xml_node_t **dub);

/** @brief dublicates the given attribute
  *
  * @param *attr - node to dublicate
  * @param **dub - dublicated value
  * @returns 0 on success
  */
int xml_node_attr_dublicate (xml_node_attr_t *attr, xml_node_attr_t **dub);

/** @brief return the parent of node with the name "name"
  *
  * @param *node - the node
  * @param *name - name of the parent
  * @returns the parent, or NULL if not found
  */
xml_node_t * xml_node_get_parent (xml_node_t *node, char *name);

/** @brief initializes the attribute struct
  *
  * @param **attr - out value
  * @returns 0 on success
  */
int xml_node_attr_init (xml_node_attr_t **attr);

/** @brief uninitializes the attribute node
  *
  * @param *attr - the attribute
  * @returns 0 on success
  */
int xml_node_attr_uninit (xml_node_attr_t *attr);

/** @brief initializes the node struct
  *
  * @param **node - out value
  * @returns 0 on success
  */
int xml_node_init (xml_node_t **node);

/** @brief uninitializes the node
  *
  * @param *node - the node
  * @returns 0 on success
  */
int xml_node_uninit (xml_node_t *node);

/** @brief parses the given buffer
  *
  * @param *buffer - buffer to parse
  * @param len     - buffer length
  * @returns node on success, NULL on error
  */
xml_node_t * xml_parse_buffer (const char *buffer, unsigned int len);

/** @brief dumps given node to buffer
  *
  * @param *node - node to dump
  * @returns buffer on success, NULL on error
  */
char * xml_node_print (const xml_node_t *node);

/*@}*/
