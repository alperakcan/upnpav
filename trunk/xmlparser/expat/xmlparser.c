
#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "platform.h"
#include "xmlparser.h"

#include "expat.h"

typedef struct xml_data_s {
	char *path;
	xml_node_t *active;
	xml_node_t *root;
	xml_node_t *elem;
} xml_data_t;

static int xml_node_path_normalize (char *out, int len);
static xml_node_t * xml_node_get_path_ (xml_node_t *node, const char *path);
static void xml_parse_start (void *xdata, const char *el, const char **xattr);
static void xml_parse_end (void *xdata, const char *el);
static void xml_parse_character_fixup (char *out);
static void xml_parse_character (void *xdata, const char *txt, int txtlen);

static int xml_node_path_normalize (char *out, int len)
{
	int i;
	int j;
	int first;
	int next;
	/* First append "/" to make the rest easier */
	if (!strncat(out,"/",len))
		return -10;
	/* Convert "//" to "/" */
	for (i = 0; out[i + 1]; i++) {
		if (out[i] == '/' && out[i + 1] == '/') {
			for (j = i + 1; out[j]; j++)
				out[j] = out[j + 1];
			i--;
		}
	}
	/* Convert "/./" to "/" */
	for (i = 0; out[i] && out[i + 1] && out[i + 2]; i++) {
		if (out[i] == '/' && out[i + 1] == '.' && out[i + 2] == '/') {
			for (j = i + 1; out[j]; j++)
				out[j] = out[j + 2];
			i--;
		}
	}
	/* Convert "/asdf/../" to "/" until we can't anymore.  Also
	 * convert leading "/../" to "/" */
	first = next = 0;
	while (1) {
		/* If a "../" follows, remove it and the parent */
		if (out[next + 1] && out[next + 1] == '.' &&
		    out[next + 2] && out[next + 2] == '.' &&
		    out[next + 3] && out[next + 3] == '/') {
			for (j = 0; out[first + j + 1]; j++)
				out[first + j + 1] = out[next + j + 4];
			first = next = 0;
			continue;
		}

		/* Find next slash */
		first = next;
		for (next = first + 1; out[next] && out[next] != '/'; next++)
			continue;
		if (!out[next])
			break;
	}
	/* Remove trailing "/" */
	for (i = 1; out[i]; i++)
		continue;
	if (i >= 1 && out[i - 1] == '/')
		out[i - 1] = 0;
	return 0;
}

static xml_node_t * xml_node_get_path_ (xml_node_t *node, const char *path)
{
	char *ptr;
	char *str;
	xml_node_t *res;
	xml_node_t *tmp;
	res = NULL;
	if (path[0] == '/') {
		path++;
		while (node->parent) {
			node = node->parent;
		}
		str = strchr(path, '/');
		if (str == NULL &&
		    strcmp(node->name, path) == 0) {
			path = str + 1;
			goto ok;
		}
		if (str != NULL &&
		    strncmp(node->name, path, str - path) == 0 &&
		    strlen(node->name) == str - path) {
			path = str + 1;
			goto ok;
		}
		goto end;
	}
ok:
	list_for_each_entry(tmp, &node->nodes, head) {
		str = strchr(path,  '/');
		if (str == NULL) {
			if (strcmp(tmp->name, path) == 0) {
				res = tmp;
				break;
			}
		} else {
			if (strncmp(tmp->name, path, str - path) == 0 &&
			    strlen(tmp->name) == str - path) {
			    	ptr = strchr(path, '/');
			    	if (ptr != NULL) {
			    		ptr++;
			    	}
			    	if ((res = xml_node_get_path_(tmp, ptr)) != NULL) {
			    		break;
			    	}
			}
		}
	}
end:	return res;
}

xml_node_t * xml_node_get_path (xml_node_t *node, const char *path)
{
	int len;
	char *str;
	xml_node_t *res;
	if (node == NULL || path == NULL) {
		return NULL;
	}
	len = strlen(path);
	str = (char *) malloc(len + 10 + 1);
	if (str == NULL) {
		return NULL;
	}
	memset(str, 0, len + 10 + 1);
	memcpy(str, path, len);
	xml_node_path_normalize(str, len + 10);
	res = xml_node_get_path_(node, str);
	free(str);
	return res;
}

char * xml_node_get_value (xml_node_t *node)
{
	return node->value;
}

char * xml_node_get_path_value (xml_node_t *node, const char *path)
{
	xml_node_t *res;
	res = xml_node_get_path(node, path);
	if (res == NULL) {
		return NULL;
	}
	return xml_node_get_value(res);
}

xml_node_attr_t * xml_node_get_attr (xml_node_t *node, const char *attr)
{
	xml_node_attr_t *tmp;
	if (node == NULL || attr == NULL) {
		return NULL;
	}
	list_for_each_entry(tmp, &node->attrs, head) {
		if (strcmp(tmp->name, attr) == 0) {
			return tmp;
		}
	}
	return NULL;
}

char * xml_node_get_attr_value (xml_node_t *node, const char *attr)
{
	xml_node_attr_t *tmp;
	tmp = xml_node_get_attr(node, attr);
	if (tmp) {
		return tmp->value;
	}
	return NULL;
}

static void xml_node_dublicate_ (xml_node_t *node, xml_node_t *dub)
{
	xml_node_t *tmp;
	xml_node_t *dmp;
	xml_node_attr_t *atmp;
	xml_node_attr_t *admp;
	dub->name = (node->name) ? strdup(node->name) : NULL;
	dub->value = (node->value) ? strdup(node->value) : NULL;
	list_for_each_entry(atmp, &node->attrs, head) {
		xml_node_attr_dublicate(atmp, &admp);
		list_add_tail(&admp->head, &dub->attrs);
	}
	list_for_each_entry(tmp, &node->nodes, head) {
    		xml_node_init(&dmp);
    		xml_node_dublicate_(tmp, dmp);
    		list_add_tail(&dmp->head, &dub->attrs);
    		dmp->parent = dub;
	}
}

int xml_node_dublicate (xml_node_t *node, xml_node_t **dub)
{
	xml_node_init(dub);
	xml_node_dublicate_(node, *dub);
	return 0;
}

int xml_node_attr_dublicate (xml_node_attr_t *attr, xml_node_attr_t **dub)
{
	xml_node_attr_init(dub);
	(*dub)->name = (attr->name) ? strdup(attr->name) : NULL;
	(*dub)->value = (attr->value) ? strdup(attr->value) : NULL;
	return 0;
}

xml_node_t * xml_node_get_parent (xml_node_t *node, char *name)
{
	while (node->parent) {
		if (strcmp(node->parent->name, name) == 0) {
			return node->parent;
		}
		node = node->parent;
	}
	return NULL;
}

int xml_node_attr_init (xml_node_attr_t **attr)
{
	xml_node_attr_t *a;
	a = (xml_node_attr_t *) malloc(sizeof(xml_node_attr_t));
	if (a == NULL) {
		return -1;
	}
	memset(a, 0, sizeof(xml_node_attr_t));
	*attr = a;
	return 0;
}

int xml_node_attr_uninit (xml_node_attr_t *attr)
{
	free(attr->name);
	free(attr->value);
	free(attr);
	return 0;
}

int xml_node_init (xml_node_t **node)
{
	xml_node_t *n;
	n = (xml_node_t *) malloc(sizeof(xml_node_t));
	if (n == NULL) {
		return -1;
	}
	memset(n, 0, sizeof(xml_node_t));
	list_init(&n->attrs);
	list_init(&n->nodes);
	*node = n;
	return 0;
}

int xml_node_uninit (xml_node_t *node)
{
	xml_node_t *n, *n_;
	xml_node_attr_t *a, *a_;
	if (node == NULL) {
		return 0;
	}
	list_for_each_entry_safe(n, n_, &node->nodes, head) {
		list_del(&n->head);
		xml_node_uninit(n);
	}
	list_for_each_entry_safe(a, a_, &node->attrs, head) {
		list_del(&a->head);
		xml_node_attr_uninit(a);
	}
	free(node->name);
	free(node->value);
	free(node);
	return 0;
}

static void xml_parse_start (void *xdata, const char *el, const char **xattr)
{
	int p;
	xml_data_t *data;
	xml_node_t *node;
	xml_node_attr_t *attr;
	data = (xml_data_t *) xdata;
	free(data->path);
	data->path = (char *) strdup(el);
	xml_node_init(&node);
	node->name = (char *) strdup(el);
	for (p = 0; xattr[p] && xattr[p + 1]; p += 2) {
		xml_node_attr_init(&attr);
		attr->name = (char *) strdup(xattr[p]);
		attr->value = (char *) strdup(xattr[p + 1]);
		list_add_tail(&attr->head, &node->attrs);
	}
	if (data->active) {
		list_add_tail(&node->head, &data->active->nodes);
	} else {
		data->root = node;
	}
	node->parent = data->active;
	data->active = node;
}

static void xml_parse_end (void *xdata, const char *el)
{
	xml_data_t *data;
	data = (xml_data_t *) xdata;
	free(data->path);
	data->path = NULL;
	data->active = data->active->parent;
}

static void xml_parse_character_fixup (char *out)
{
	int i;
	int j;
	/* Convert "&amp;" to "&" */
	for (i = 0; out[i] && out[i + 1] && out[i + 2] && out[i + 3] && out[i + 4]; i++) {
		if (out[i] == '&' && out[i + 1] == 'a' && out[i + 2] == 'm' && out[i + 3] == 'p' && out[i + 4] == ';') {
			for (j = i + 1; out[j]; j++)
				out[j] = out[j + 4];
			i--;
		}
	}
}

static void xml_parse_character (void *xdata, const char *txt, int txtlen)
{
	char *str;
	xml_data_t *data;
	unsigned int total = 0;
	unsigned int total_old = 0;
	data = (xml_data_t *) xdata;
	if (txtlen > 0 && txt && data->path) {
	} else {
		return;
	}
	if (data->active == NULL) {
		return;
	}
	if (data->active->value != NULL) {
		total_old = strlen(data->active->value);
	}
	total = (total_old + txtlen + 1) * sizeof(char);
	data->active->value = (char *) realloc(data->active->value, total);
	if (total_old == 0) {
		data->active->value[0] = '\0';
	}
	strncat(data->active->value, txt, txtlen);
	str = data->active->value;
	if (data->path) {
	    	if (data->active && data->active->value) {
	    		xml_parse_character_fixup(data->active->value);
	    	}
	}
}

int xml_parse_buffer (xml_node_t **node, char *buffer, unsigned int len)
{
	int ret;
	XML_Parser p;
	xml_data_t *data;
	ret = 0;
	data = (xml_data_t *) malloc(sizeof(xml_data_t));
	if (data == NULL) {
		return -1;
	}
	memset(data, 0, sizeof(xml_data_t));
	p = XML_ParserCreate(NULL);
	XML_SetUserData(p, data);
	XML_SetElementHandler(p, xml_parse_start, xml_parse_end);
	XML_SetCharacterDataHandler(p, xml_parse_character);
	if (!XML_Parse(p, buffer, len, 1)) {
		debugf(0, "Parse error at line %d:%s", (int) XML_GetCurrentLineNumber(p), XML_ErrorString(XML_GetErrorCode(p)));
		ret = -1;
	}
	XML_ParserFree(p);
	*node = data->root;
	free(data);
	return ret;
}
