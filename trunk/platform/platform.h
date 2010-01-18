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

/** @addtogroup platform_socket */
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

/**
 * @brief receive data from stream socket
 *
 * @param *socket - socket object
 * @param *buffer - recv buffer
 * @param length  - maximum data length to read
 *
 * @returns received buffer size on success, -1 on error
 */
int socket_recv (socket_t *socket, void *buffer, int length);

/**
 * @brief send data from stream socket
 *
 * @param *socket - socket object
 * @param *buffer - send buffer
 * @param length  - length of buffer
 *
 * @returns sent buffer size on success, -1 on error
 */
int socket_send (socket_t *socket, const void *buffer, int length);

/**
 * @brief receive data from datagram socket
 *
 * @param *socket  - socket object
 * @param *buf     - receive buffer
 * @param length   - maximum data length to read
 * @param *address - preallocated sender address, can be NULL
 * @param *port    - sender port, can be NULL
 *
 * @returns received buffer size, sender address, sender port on success, -1 on error
 */
int socket_recvfrom (socket_t *socket, void *buf, int length, char *address, int *port);

/**
 * @bried send data from datagram socket to given address, port
 *
 * @param *socket  - socket object
 * @param *buf     - send buffer
 * @param length   - length of buffer
 * @param *address - destination address
 * @param port     - destination port
 *
 * @return send buffer size on success, -1 on error
 */
int socket_sendto (socket_t *socket, const void *buf, int length, const char *address, int port);

/**
 * @brief poll events on given socket tiwh a given timeout value
 *
 * @param *socket - socket object
 * @param request - requested events (bitwise or'ed)
 * @param *result - result events (bitwise or'ed)
 * @param timeout - timeout to wait on socket in miliseconds, -1 for infinite
 *
 * @returns 0 on timeout, 1 on success, -1 on error
 */
int socket_poll (socket_t *socket, socket_event_t request, socket_event_t *result, int timeout);

/**
 * @brief closes and destroys given socket object
 *
 * @param *socket - socket object
 *
 * @return 0 on success, -1 on error
 */
int socket_close (socket_t *socket);

/**
 * @brief sets reusable flag for given socket object
 *
 * @param *socket - socket object
 * @param on      - 0 for disable, 1 for enable
 *
 * @returns 0 on success, -1 on error
 */
int socket_option_reuseaddr (socket_t *socket, int on);

/**
 * @brief joins/leaves to a given multicast address
 *
 * @param *socket  - socket object
 * @param *address - destination multicast address
 * @param on       - 0 for leave, 1 for join
 *
 * @returns 0 on success, -1 on error
 */
int socket_option_membership (socket_t *socket, const char *address, int on);

/**
 * @brief sets multicast time to live value for given socket
 *
 * @param *socket - socket object
 * @param ttl     - time to live value
 *
 * @returns 0 on success, -1 on error
 */
int socket_option_multicastttl (socket_t *socket, int ttl);

/**
 * @brief  converts the Internet host address from the IPv4 numbers-and-dots
 *         notation into binary form (in network byte order).
 *
 * @param address - address to convert
 * @param baddress - binary representation of address
 *
 * @returns 0 on success, -1 on error
 */
int socket_inet_aton (const char *address, unsigned int *baddress);

/*@}*/

#ifndef FILE_MAX_LENGTH
/**
 * @brief maximum allowed length for file name
 */
#define FILE_MAX_LENGTH 1024
#endif

/**
 * @brief file open modes
 */
typedef enum {
	/** read mode */
	FILE_MODE_READ  = 0x01,
	/** write mode */
	FILE_MODE_WRITE = 0x02,
} file_mode_t;

/**
 * @brief file seek operations
 */
typedef enum {
	/** seek from beginning */
	FILE_SEEK_SET = 0x00,
	/** seek from current position */
	FILE_SEEK_CUR = 0x01,
	/** seek from end of file */
	FILE_SEEK_END = 0x02,
} file_seek_t;

/**
 * @brief file types
 */
typedef enum {
	/** unknown type */
	FILE_TYPE_UNKNOWN   = 0x00,
	/** regular file */
	FILE_TYPE_REGULAR   = 0x01,
	/** directory */
	FILE_TYPE_DIRECTORY = 0x02,
} file_type_t;

/**
 * @brief filename match options
 */
typedef enum {
	/** ignore case sensitive matching */
	FILE_MATCH_CASEFOLD = 0x01,
} file_match_t;

/**
 * @brief exported file structure
 */
typedef struct file_s file_t;

/**
 * @brief exported dir structure
 */
typedef struct dir_s dir_t;

/**
 * @brief file stat information
 */
typedef struct file_stat_s {
	/** file size */
	unsigned long long size;
	/** file modification time */
	unsigned int mtime;
	/** file type */
	file_type_t type;
} file_stat_t;

/**
 * @brief directory entry information
 */
typedef struct dir_entry_s {
	/** entry name */
	char name[FILE_MAX_LENGTH];
} dir_entry_t;

/**
 * @brief tests if file is a regular file
 */
#define FILE_ISREG(type) (type & FILE_TYPE_REGULAR)

/**
 * @brief tests if file is a directory
 */
#define FILE_ISDIR(type) (type & FILE_TYPE_DIRECTORY)

int file_match (const char *path, const char *string, file_match_t flag);
int file_access (const char *path, file_mode_t mode);
int file_stat (const char *path, file_stat_t *stat);
file_t * file_open (const char *path, file_mode_t mode);
int file_read (file_t *file, void *buffer, int length);
int file_write (file_t *file, const void *buffer, int length);
unsigned long long file_seek (file_t *file, unsigned long long offset, file_seek_t whence);
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
char * interface_getmask (char *ifname);
int interface_printall (void);

extern int platform_debug;
#define debugf(fmt...) debug_debugf(__FILE__, __LINE__, __FUNCTION__, fmt);
void debug_debugf (char *file, int line, const char *func, char *fmt, ...);

typedef struct list_s list_t;

struct list_s {
	struct list_s *next;
	struct list_s *prev;
};

/* link list derived from linux kernel */

#define offsetof_(type, member) ((size_t) &((type *) 0)->member)

#define containerof_(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof_(type,member) );})

#define list_entry(ptr, type, member) \
	containerof_(ptr, type, member)

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
	     n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

static inline int list_count (const list_t *head)
{
	int count;
	list_t *entry;
	count = 0;
	list_for_each(entry, head) {
		count++;
	}
	return count;
}

static inline void list_add_actual (list_t *new, list_t *prev, list_t *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_del_actual (list_t *prev, list_t *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_add_tail (list_t *new, list_t *head)
{
	list_add_actual(new, head->prev, head);
}

static inline void list_add (list_t *new, list_t *head)
{
	list_add_actual(new, head, head->next);
}

static inline void list_del (list_t *entry)
{
	list_del_actual(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

static inline int list_is_first (list_t *list, list_t *head)
{
	return head->next == list;
}

static inline int list_is_last (list_t *list, list_t *head)
{
	return list->next == head;
}

static inline void list_init (list_t *head)
{
	head->next = head;
	head->prev = head;
}
