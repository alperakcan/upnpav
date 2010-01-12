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
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>

#include "platform.h"

typedef struct thread_arg_s thread_arg_t;

struct thread_arg_s {
        char *name;
        int flag;
        void *arg;
        void * (*r) (void *);
        void * (*f) (void *);
        thread_cond_t *cond;
        thread_mutex_t *mut;
};

struct thread_s {
        char *name;
        pthread_t thread;
};

struct thread_cond_s {
	char *name;
        pthread_cond_t cond;
};

struct thread_mutex_s {
	char *name;
	int recursive;
        pthread_mutex_t mutex;
};

thread_cond_t * thread_cond_init (const char *name)
{
        thread_cond_t *c;
        c = (thread_cond_t *) malloc(sizeof(thread_cond_t));
        if (c == NULL) {
                return NULL;
        }
        memset(c, 0, sizeof(thread_cond_t));
        c->name = strdup(name);
        if (pthread_cond_init(&c->cond, NULL) != 0) {
                free(c);
                return NULL;
        }
        return c;
}

int thread_cond_destroy (thread_cond_t *cond)
{
        pthread_cond_destroy(&cond->cond);
        free(cond->name);
        free(cond);
        return 0;
}

int thread_cond_signal (thread_cond_t *cond)
{
        return pthread_cond_signal(&cond->cond);
}

int thread_cond_broadcast (thread_cond_t *cond)
{
	return pthread_cond_broadcast(&cond->cond);
}

int thread_cond_wait (thread_cond_t *cond, thread_mutex_t *mutex)
{
        return pthread_cond_wait(&cond->cond, &mutex->mutex);
}

int thread_cond_timedwait (thread_cond_t *cond, thread_mutex_t *mut, int msec)
{
        int ret;
        struct timeval tval;
        struct timespec tspec;

        if (msec < 0) {
                return thread_cond_wait(cond, mut);
        }

        gettimeofday(&tval, NULL);
        tspec.tv_sec = tval.tv_sec + (msec / 1000);
        tspec.tv_nsec = (tval.tv_usec + ((msec % 1000) * 1000)) * 1000;

        if (tspec.tv_nsec >= 1000000000) {
                tspec.tv_sec += 1;
                tspec.tv_nsec -= 1000000000;
        }

again:  ret = pthread_cond_timedwait(&cond->cond, &mut->mutex, &tspec);
        switch (ret) {
                case EINTR:
                        goto again;
                        break;
                case ETIMEDOUT:
                        ret = 1;
                        break;
                case 0:
                        break;
                default:
                        assert(0);
                        ret = -1;
                        break;
        }
        return ret;
}

thread_mutex_t * thread_mutex_init (const char *name, int recursive)
{
        thread_mutex_t *m;
        m = (thread_mutex_t *) malloc(sizeof(thread_mutex_t));
        if (m == NULL) {
                return NULL;
        }
        memset(m, 0, sizeof(thread_mutex_t));
        m->name = strdup(name);
        if (recursive != 0) {
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
                if (pthread_mutex_init(&m->mutex, &attr) != 0) {
                	free(m->name);
                        free(m);
                        return NULL;
                }
                pthread_mutexattr_destroy(&attr);
                m->recursive = 1;
        } else {
                if (pthread_mutex_init(&m->mutex, NULL) != 0) {
                	free(m->name);
                        free(m);
                        return NULL;
                }
        }
        return m;
}

int thread_mutex_destroy (thread_mutex_t *mutex)
{
        pthread_mutex_destroy(&mutex->mutex);
        free(mutex->name);
        free(mutex);
        return 0;
}

int thread_mutex_lock (thread_mutex_t *mutex)
{
	int r;
	r = pthread_mutex_lock(&mutex->mutex);
	return r;
}

int thread_mutex_unlock (thread_mutex_t *mutex)
{
	int r;
	r = pthread_mutex_unlock(&mutex->mutex);
	return r;
}

static void * thread_run (void *farg)
{
        thread_arg_t *arg = (thread_arg_t *) farg;
        void *p = arg->arg;
        void * (*f) (void *) = arg->f;

        thread_mutex_lock(arg->mut);
        arg->flag = 1;
        thread_cond_signal(arg->cond);
        thread_mutex_unlock(arg->mut);

        f(p);

        return NULL;
}

thread_t * thread_create (const char *name, void * (*function) (void *), void *farg)
{
        int ret;
        thread_t *tid;
        thread_arg_t *arg;

        tid = (thread_t *) malloc(sizeof(thread_t));
        arg = (thread_arg_t *) malloc(sizeof(thread_arg_t));
        tid->name = strdup(name);

        arg->r = &thread_run;
        arg->f = function;
        arg->arg = farg;
        arg->name = tid->name;
        arg->cond = thread_cond_init("arg->cond");
        arg->mut = thread_mutex_init("arg->mut", 0);
        arg->flag = 0;

        thread_mutex_lock(arg->mut);
        ret = pthread_create(&(tid->thread), NULL, arg->r, arg);
        if (ret != 0) {
                goto out;
        }
        while (arg->flag != 1) {
                thread_cond_wait(arg->cond, arg->mut);
        }
out:
        thread_mutex_unlock(arg->mut);

        thread_cond_destroy(arg->cond);
        thread_mutex_destroy(arg->mut);
        free(arg);
        arg = NULL;

        return tid;
}

int thread_join (thread_t *thread)
{
        free(thread->name);
        pthread_join(thread->thread, NULL);
        free(thread);
        return 0;
}

unsigned int thread_self (void)
{
	return (unsigned int) pthread_self();
}

int thread_sched_yield (void)
{
        return sched_yield();
}
