/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _RB_H
#define _RB_H

#ifdef _RB_USE_GLIB
#include <glib.h>

typedef GMutex rb_mutex_t;
#define _RB_LOCK(L) g_mutex_lock(L)
#define _RB_UNLOCK(L) g_mutex_unlock(L)

#else
#include <pthread.h>

typedef pthread_mutex_t rb_mutex_t;
#define _RB_LOCK(L) pthread_mutex_lock(L)
#define _RB_UNLOCK(L) pthread_mutex_unlock(L)
#endif

#include <stdlib.h>

#ifdef RB_DEBUG
#define ASSERT_RB(buf) _assert_rb(buf)
#else
#define ASSERT_RB(buf)
#endif

struct ringbuf {
    rb_mutex_t* lock;
    char _free_lock;
    char* buf;
    char* end;
    char* wp;
    char* rp;
    unsigned int free;
    unsigned int used;
    unsigned int size;
};

int init_rb(struct ringbuf* rb, unsigned int size);
int init_rb_with_lock(struct ringbuf* rb, unsigned int size, rb_mutex_t* lock);
int write_rb(struct ringbuf* rb, void* buf, unsigned int size);
int read_rb(struct ringbuf* rb, void* buf, unsigned int size);
int read_rb_locked(struct ringbuf* rb, void* buf, unsigned int size);
void reset_rb(struct ringbuf* rb);
unsigned int free_rb(struct ringbuf* rb);
unsigned int free_rb_locked(struct ringbuf* rb);
unsigned int used_rb(struct ringbuf* rb);
unsigned int used_rb_locked(struct ringbuf* rb);
void destroy_rb(struct ringbuf* rb);

#endif
