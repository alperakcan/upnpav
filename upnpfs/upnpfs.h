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

#ifndef UPNPFS_H_
#define UPNPFS_H_

#define _GNU_SOURCE

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>

#include <fuse.h>

#include "platform.h"
#include "parser.h"
#include "gena.h"
#include "upnp.h"
#include "common.h"

#if !defined(FUSE_VERSION) || (FUSE_VERSION < 26)
#error "***********************************************************"
#error "*                                                         *"
#error "*     Compilation requires at least FUSE version 2.6.0!   *"
#error "*                                                         *"
#error "***********************************************************"
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct upnpfs_cache_s {
	list_t head;
	char *path;
	char *device;
	char *object;
	int container;
	uint64_t size;
	char *source;
} upnpfs_cache_t;

typedef struct upnpfs_file_s {
	int metadata;
	upnpfs_cache_t *cache;
	void *protocol;
} upnpfs_file_t;

struct options_s {
	int debug;
	int silent;
	char *mntpoint;
	char *interface;
	char *options;
	int cache_size;
};

struct private_s {
	char *name;
	char *options;
	client_t *controller;
	list_t cache;
	unsigned int cache_size;
	thread_mutex_t *cache_mutex;
};

extern struct options_s opts;
extern struct private_s priv;

static inline void debugfs_printf (const char *function, char *file, int line, const char *fmt, ...)
{
	va_list args;
	if (opts.debug == 0 || opts.silent == 1) {
		return;
	}
	fprintf(stderr, "%s: ", PACKAGE_NAME);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, " [%s (%s:%d)]\n", function, file, line);
	fflush(stderr);
}

#define debugfs(a...) { \
	debugfs_printf(__FUNCTION__, __FILE__, __LINE__, a); \
}

int do_releasefile (upnpfs_file_t *file);

int do_releasecache (upnpfs_cache_t *cache);

upnpfs_cache_t * do_findcache (const char *path);

upnpfs_cache_t * do_insertcache (const char *path, const char *device, entry_t *entry);

upnpfs_cache_t * do_findpath (const char *path);

int op_access (const char *path, int mask);

void * op_init (struct fuse_conn_info *conn);

void op_destroy (void *userdata);

int op_opendir (const char *path, struct fuse_file_info *fi);

int op_readdir (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

int op_getattr (const char *path, struct stat *stbuf);

int op_open (const char *path, struct fuse_file_info *fi);

int op_read (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int op_release (const char *path, struct fuse_file_info *fi);

#endif /* UPNPFS_H_ */
