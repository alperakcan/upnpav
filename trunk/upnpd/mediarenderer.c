#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#if HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "upnpd.h"

#include "mediarenderer.h"
#include "platform.h"

typedef enum {
	OPT_INTERFACE    = 0,
	OPT_FRIENDLYNAME = 1,
} mediarenderer_options_t;

static char *mediarenderer_options[] = {
	"interface",
	"friendlyname",
	NULL,
};

#if !HAVE_LIBREADLINE
static int rinit = 0;
static list_t rlist;

static char * readline (const char *shell)
{
	char *buf;
	size_t size;
	size_t read;
	if (rinit == 0) {
		list_init(&rlist);
		rinit = 1;
	}
	printf("%s ", shell);
	buf = NULL;
	size = 0;
	read = getdelim(&buf, &size, '\n', stdin);
	if (read == -1) {
		return NULL;
	}
	buf[read - 1] = '\0';
	return buf;
}

static int add_history (const char *line)
{
	if (rinit == 0) {
		list_init(&rlist);
		rinit = 1;
	}
	return 0;
}
#endif

static int mediarenderer_rline_process (upnpavd_mediarenderer_t *mediarenderer, char *command)
{
	int ret;
	char *b;
	char *p;
	int argc;
	char **argv;

	ret = 0;
	argc = 0;
	argv = NULL;
	b = strdup(command);
	p = b;

	while (*p) {
		while (isspace (*p)) {
			p++;
		}

		if (*p == '"' || *p == '\'') {
			char const delim = *p;
			char *const begin = ++p;

			while (*p && *p != delim) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else {
				goto out;
			}
		} else {
			char *const begin = p;

			while (*p && ! isspace (*p)) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else if (p != begin) {
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			}
		}
	}

	if (strcmp(argv[0], "help") == 0) {
		printf("help                                - this text\n"
		       "quit                                - quit mediarenderer\n"
		       "refresh                             - refresh device list\n"
		       "list                                - list all devices\n"
		       "browse <objectid>                   - browse a device\n"
		       "metadata <objectid>                 - print metadata information of an object\n");
	} else if (strcmp(argv[0], "quit") == 0) {
		ret = -1;
	}

out:	free(argv);
	free(b);
	return ret;
}

static char * mediarenderer_rline_strip (char *buf)
{
	char *start;

	/* Strip off trailing whitespace, returns, etc */
	while ((*buf != '\0') && (buf[strlen(buf) - 1] < 33)) {
		buf[strlen(buf) - 1] = '\0';
	}
	start = buf;

	/* Strip off leading whitespace, returns, etc */
	while (*start && (*start < 33)) {
		start++;
	}

	return start;
}

static int mediarenderer_main (char *options)
{
	int rc;
	int err;
	int ret;
	int running;
	char *rline;
	char *value;
	char *rcommand;
	char *interface;
	char *friendlyname;
	char *suboptions;
	upnpavd_mediarenderer_t *mediarenderer;

	ret = -1;
	err = 0;
	interface = NULL;
	friendlyname = NULL;

	suboptions = options;
	while (suboptions && *suboptions != '\0' && !err) {
		switch (getsubopt(&suboptions, mediarenderer_options, &value)) {
			case OPT_INTERFACE:
				if (value == NULL) {
					printf("value is missing for interface option\n");
					err = 1;
					continue;
				}
				interface = value;
				break;
			case OPT_FRIENDLYNAME:
				if (value == NULL) {
					printf("value is missing for friendlyname option\n");
					err = 1;
					continue;
				}
				friendlyname = value;
				break;
		}
	}

	mediarenderer = upnpavd_mediarenderer_init(interface, friendlyname);
	if (mediarenderer == NULL) {
		printf("upnpd_mediarenderer_init() failed\n");
		goto out;
	}

	running = 1;
	while (running) {
		rline = readline("console> ");
		rcommand = mediarenderer_rline_strip(rline);
		if (rcommand && *rcommand) {
			add_history(rcommand);
			if (mediarenderer_rline_process(mediarenderer, rcommand) != 0) {
				running = 0;
			}
		}
		free(rline);
	};

	rc = upnpavd_mediarenderer_uninit(mediarenderer);
	if (rc != 0) {
		printf("upnpd_mediarenderer_uninit(mediarenderer) failed\n");
		goto out;
	}
	ret = 0;
out:	return ret;
}

upnpd_application_t mediarenderer = {
	"mediarenderer",
	"upnp av mediarenderer device",
	&mediarenderer_main,
};
