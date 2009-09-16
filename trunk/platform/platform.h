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

/**
 * @defgroup platform platform api
 * @brief    platform specific porting layer api definitions
 */

/**
 * @defgroup platform_thread platform thread api
 * @ingroup  platform
 * @brief    thread abstraction for platform layer
 */

/** @addtogroup platform_thread */
/*@{*/

/**
 * @brief exported thread structure
 */
typedef struct thread_s thread_t;

/**
 * @brief exported thread condition variable structure
 */
typedef struct thread_cond_s thread_cond_t;

/**
 * @brief exported thread mutex structure
 */
typedef struct thread_mutex_s thread_mutex_t;

/**
 * @brief creates a new thread of control that executes concurrently
 *        with the calling thread. the new thread applies the function
 *        passing it as first argument.
 *
 * @param *name     - thread name
 * @param *function - thread function.
 * @param *arg      - argument passed to thread function.
 *
 * @returns NULL on error, otherwise the thread object
 */
thread_t * thread_create (const char *name, void * (*function) (void *), void *arg);

/**
 * @brief returns the thread identifier for the calling thread.
 *
 * @returns thread id.
 */
unsigned int thread_self (void);

/**
 * @brief suspends the execution of the calling thread until the
 *        thread terminates, either by calling thread_exit or by
 *        being cancelled.
 *
 * @param *thread  - the thread
 *
 * @returns 0 on success, 1 on error.
 */
int thread_join (thread_t *thread);

/**
 * @brief initialize the mutex object
 *
 * @param *name     - name of the mutex
 * @param recursive - 1 if recursive otherwise 0
 *
 * @returns NULL on error, otherwise the mutex object
 */
thread_mutex_t * thread_mutex_init (const char *name, int recursive);

/**
 * @brief locks the given mutex
 *
 * @param *mutex - the mutex
 *
 * @returns 0 on success, 1 on error.
 */
int thread_mutex_lock (thread_mutex_t *mutex);

/**
 * @brief unlocks the given mutex
 *
 * @param *mutex - the mutex
 *
 * @returns 0 on success, 1 on error.
 */
int thread_mutex_unlock (thread_mutex_t *mutex);

/**
 * @brief destroys the given mutex
 *
 * @param *mutex - the mutex
 *
 * @returns 0 on success, 1 on error.
 */
int thread_mutex_destroy (thread_mutex_t *mutex);

/**
 * @brief initialize condition variable object
 *
 * @param *name - name of the condition variable
 *
 * @returns NULL on error, otherwise the condition variable object
 */
thread_cond_t * thread_cond_init (const char *name);

/**
 * @brief wait on condition variable by automatically unocking the
 *        mutex object and waiting for condition signal.
 *
 * @param *cond  - condition variable object
 * @param *mutex - mutex object
 *
 * @returns 0 on success, otherwise -1
 */
int thread_cond_wait (thread_cond_t *cond, thread_mutex_t *mutex);

/**
 * @brief wait on condition variable for a specific amount of time.
 *
 * @param *cond - condition variable object
 * @param *mutex - mutex object
 * @param timeout - timeout value in miliseconds, -1 for infinite
 *
 * @returns 0 on success, 1 on timeout, -1 on error
 */
int thread_cond_timedwait (thread_cond_t *cond, thread_mutex_t *mutex, int timeout);

/**
 * @brief unlock a thread waiting for a condition variable
 *
 * @param *cond - condition variable object
 *
 * @returns 0 on success, otherwise -1
 */
int thread_cond_signal (thread_cond_t *cond);

/**
 * @brief unlocks all threads waiting for a condition variable
 *
 * @param *cond - condition variable object
 *
 * @returns 0 on success, otherwise -1
 */
int thread_cond_broadcast (thread_cond_t *cond);

/**
 * @brief destroys given condition variable object
 *
 * @param *cond - condition variable object
 *
 * @returns 0 on success, otherwise -1
 */
int thread_cond_destroy (thread_cond_t *cond);

/*@}*/

/**
 * @defgroup platform_socket platform socket api
 * @ingroup  platform
 * @brief    socket abstraction for platform layer
 */

/** @addtogroup platform_thread */
/*@{*/

#ifndef SOCKET_IP_LENGTH
/**
 * @brief maximum allowed length for IP string
 */
#define SOCKET_IP_LENGTH 20
#endif

/**
 * @brief socket communication types
 */
typedef enum {
	/** tcp communication */
	SOCKET_TYPE_STREAM = 0x00,
	/** udp communication */
	SOCKET_TYPE_DGRAM  = 0x01,
} socket_type_t;

/**
 * @brief socket event types
 */
typedef enum {
	/** in data event */
	SOCKET_EVENT_IN   = 0x01,
	/** out data event */
	SOCKET_EVENT_OUT  = 0x02,
	/** error event */
	SOCKET_EVENT_ERR  = 0x04,
} socket_event_t;

/**
 * @brief exported socket structure
 */
typedef struct socket_s socket_t;

/**
 * @brief create a socket object with given socket type
 *
 * @param type - socket type
 *
 * @returns socket object on success, otherwise NULL
 */
socket_t * socket_open (socket_type_t type);

/**
 * @brief binds a socket to a given address and port
 *
 * @param * socket - socket object
 * @param *address - bind address
 * @param port     - bind port
 *
 * @returns 0 on success, otherwise -1
 */
int socket_bind (socket_t *socket, const char *address, int port);

/**
 * @brief sets maximum allowed connection for a given socket object
 *
 * @param *socket - socket object
 * @param backlog - maximum allowed connection
 *
 * @returns 0 on success, otherwise -1
 */
int socket_listen (socket_t *socket, int backlog);

/**
 * @brief accepts a new connection on given socket object
 *
 * @param *socket - socket object
 *
 * @returns new socket object for accepted connection, otherwise NULL
 */
socket_t * socket_accept (socket_t *socket);

/**
 * @brief connects to given address and port with given timeout value
 *
 * @param *socket  - socket object
 * @param *address - remote address
 * @param port     - remote port
 * @param timeout  - timeout value in miliseconds, -1 for infinite
 *
 * @returns 0 on success, otherwise -1
 */
int socket_connect (socket_t *socket, const char *address, int port, int timeout);
int socket_recv (socket_t *socket, void *buffer, int length);
int socket_send (socket_t *socket, const void *buffer, int length);
int socket_recvfrom (socket_t *socket, void *buf, int length, char *address, int *port);
int socket_sendto (socket_t *socket, const void *buf, int length, const char *address, int port);
int socket_poll (socket_t *socket, socket_event_t request, socket_event_t *result, int timeout);
int socket_close (socket_t *socket);

int socket_option_reuseaddr (socket_t *socket, int on);
int socket_option_membership (socket_t *socket, const char *address, int on);
int socket_option_multicastttl (socket_t *socket, int ttl);

/*@}*/

#ifndef FILE_MAX_LENGTH
#define FILE_MAX_LENGTH 1024
#endif

typedef enum {
	FILE_MODE_READ  = 0x01,
	FILE_MODE_WRITE = 0x02,
} file_mode_t;

typedef enum {
	FILE_SEEK_SET = 0x00,
	FILE_SEEK_CUR = 0x01,
	FILE_SEEK_END = 0x02,
} file_seek_t;

typedef enum {
	FILE_TYPE_UNKNOWN   = 0x00,
	FILE_TYPE_REGULAR   = 0x01,
	FILE_TYPE_DIRECTORY = 0x02,
} file_type_t;

typedef enum {
	FILE_MATCH_CASEFOLD = 0x01,
} file_match_t;

typedef struct file_s file_t;
typedef struct dir_s dir_t;

typedef struct file_stat_s {
	int64_t size;
	unsigned int mtime;
	file_type_t type;
} file_stat_t;

typedef struct dir_entry_s {
	char name[FILE_MAX_LENGTH];
} dir_entry_t;

#define FILE_ISREG(type) (type & FILE_TYPE_REGULAR)
#define FILE_ISDIR(type) (type & FILE_TYPE_DIRECTORY)

int file_match (const char *path, const char *string, file_match_t flag);
int file_access (const char *path, file_mode_t mode);
int file_stat (const char *path, file_stat_t *stat);
file_t * file_open (const char *path, file_mode_t mode);
int file_read (file_t *file, void *buffer, int length);
int file_write (file_t *file, const void *buffer, int length);
long file_seek (file_t *file, long offset, file_seek_t whence);
int file_close (file_t *file);

dir_t * file_opendir (const char *path);
int file_readdir (dir_t *dir, dir_entry_t *entry);
int file_closedir (dir_t *dir);

void rand_srand (unsigned int seed);
int rand_rand (void);

void time_sleep (unsigned int secs);
void time_usleep (unsigned int usecs);
unsigned long long time_gettimeofday (void);
int time_strftime (char *str, int max, unsigned long long tm);

char * interface_getaddr (char *ifname);
int interface_printall (void);

extern int platform_debug;
#define debugf(fmt...) debug_debugf(__FILE__, __LINE__, __FUNCTION__, fmt);
void debug_debugf (char *file, int line, const char *func, char *fmt, ...);

