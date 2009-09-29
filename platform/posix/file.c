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
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <dirent.h>

#include "platform.h"

struct dir_s {
	DIR *dir;
};

struct file_s {
	file_mode_t mode;
	int fd;
};

static inline int file_mode_access (file_mode_t mode)
{
	int m;
	m = 0;
	if (mode & FILE_MODE_READ)  m |= R_OK;
	if (mode & FILE_MODE_WRITE) m |= W_OK;
	return m;
}

static inline int file_mode_open (file_mode_t mode)
{
	int m;
	m = 0;
	if ((mode & FILE_MODE_READ) == FILE_MODE_READ) {
		m = O_RDONLY;
	} else if ((mode & FILE_MODE_WRITE) == FILE_MODE_WRITE) {
		m = O_WRONLY;
	} else {
		m = O_RDWR;
	}
//	m |= O_LARGEFILE;
	return m;
}

static inline int file_whence_seek (file_seek_t whence)
{
	if (whence == FILE_SEEK_SET) return SEEK_SET;
	if (whence == FILE_SEEK_CUR) return SEEK_CUR;
	if (whence == FILE_SEEK_END) return SEEK_END;
	return SEEK_SET;
}

static inline int file_flag_match (file_match_t flag)
{
	int f;
	f = 0;
	if (flag & FILE_MATCH_CASEFOLD) f |= FNM_CASEFOLD;
	return f;
}

int file_match (const char *path, const char *string, file_match_t flag)
{
	int f;
	f = file_flag_match(flag);
	return fnmatch(path, string, f);
}

int file_access (const char *path, file_mode_t mode)
{
	int m;
	m = file_mode_access(mode);
	return access(path, m);
}

int file_stat (const char *path, file_stat_t *st)
{
	struct stat stbuf;
	if (stat(path, &stbuf) < 0) {
		return -1;
	}
	st->size = stbuf.st_size;
	st->mtime = stbuf.st_mtime;
	st->type = 0;
	if (S_ISREG(stbuf.st_mode)) st->type |= FILE_TYPE_REGULAR;
	if (S_ISDIR(stbuf.st_mode)) st->type |= FILE_TYPE_DIRECTORY;
	return 0;
}

file_t * file_open (const char *path, file_mode_t mode)
{
	int m;
	file_t *f;
	f = (file_t *) malloc(sizeof(file_t));
	if (f == NULL) {
		return NULL;
	}
	memset(f, 0, sizeof(file_t));
	m = file_mode_open(mode);
	f->mode = mode;
	f->fd = open(path, m);
	if (f->fd < 0) {
		free(f);
		return NULL;
	}
	return f;
}

int file_read (file_t *file, void *buffer, int length)
{
	return read(file->fd, buffer, length);
}

int file_write (file_t *file, const void *buffer, int length)
{
	return write(file->fd, buffer, length);
}

unsigned long long file_seek (file_t *file, unsigned long long offset, file_seek_t whence)
{
	int s;
	unsigned long long r;
	s = file_whence_seek(whence);
	r = lseek(file->fd, (unsigned long long) offset, s);
	return r;
}

int file_close (file_t *file)
{
	close(file->fd);
	free(file);
	return 0;
}

dir_t * file_opendir (const char *path)
{
	dir_t *d;
	d = (dir_t *) malloc(sizeof(dir_t));
	if (d == NULL) {
		return NULL;
	}
	d->dir = opendir(path);
	if (d->dir == NULL) {
		free(d);
		return NULL;
	}
	return d;
}

int file_readdir (dir_t *dir, dir_entry_t *entry)
{
	struct dirent *c;
	c = readdir(dir->dir);
	if (c == NULL) {
		return -1;
	}
	if (strlen(c->d_name) + 1 > sizeof(entry->name)) {
		return -1;
	}
	strncpy(entry->name, c->d_name, sizeof(entry->name));
	return 0;
}

int file_closedir (dir_t *dir)
{
	closedir(dir->dir);
	free(dir);
	return 0;
}
