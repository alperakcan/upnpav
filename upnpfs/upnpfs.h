/**
 * Copyright (c) 2008-2009 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the fuse-ext2
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef UPNPFS_H_
#define UPNPFS_H_

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>

#include <fuse.h>

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
} upnpfs_cache_t;

struct options_s {
	int debug;
	int silent;
	char *mntpoint;
	char *interface;
	char *options;
};

struct private_s {
	char *name;
	char *options;
	client_t *controller;
	list_t cache;
};

extern struct options_s opts;
extern struct private_s priv;

#if ENABLE_DEBUG || 1

static inline void debugfs_printf (const char *function, char *file, int line, const char *fmt, ...)
{
	va_list args;
	if (opts.debug == 0 || opts.silent == 1) {
		return;
	}
	printf("%s: ", PACKAGE);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf(" [%s (%s:%d)]\n", function, file, line);
}

#define debugfs(a...) { \
	debugfs_printf(__FUNCTION__, __FILE__, __LINE__, a); \
}

#else /* ENABLE_DEBUG */

#define debugf(a...) do { } while(0)

#endif /* ENABLE_DEBUG */

typedef struct upnpfs_file_s {
	char *device;
	char *object;
	char *path;
	unsigned int size;
	char *protocol;
} upnpfs_file_t;

int do_findpath (const char *path, char **device, char **object);

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
