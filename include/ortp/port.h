/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* this file is responsible of the portability of the stack */

#ifndef ORTP_PORT_H
#define ORTP_PORT_H
#include <stdio.h>

#define bool_t int
#define TRUE  1
#define FALSE 0

#ifdef __cplusplus
extern "C"{
#endif

void* ortp_malloc(size_t sz);
void ortp_free(void *ptr);
void* ortp_realloc(void *ptr, size_t sz);
void* ortp_malloc0(size_t sz);

/*override the allocator with this method, to be called BEFORE ortp_init()*/
typedef struct _OrtpMemoryFunctions
{
    void *(*malloc_fun)(size_t sz);
    void *(*realloc_fun)(void *ptr, size_t sz);
    void (*free_fun)(void *ptr);
}OrtpMemoryFunctions;

void ortp_set_memory_functions(OrtpMemoryFunctions *functions);

#define ortp_new(type,count)    (type*)ortp_malloc(sizeof(type)*(count))
#define ortp_new0(type,count)   (type*)ortp_malloc0(sizeof(type)*(count))

#ifdef __cplusplus
}
#endif

#endif
