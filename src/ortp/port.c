
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

#include <stdlib.h>
#include <string.h>
#include "ortp/port.h"

static void *ortp_libc_malloc(size_t sz)
{
    return malloc(sz);
}

static void *ortp_libc_realloc(void *ptr, size_t sz)
{
    return realloc(ptr,sz);
}

static void ortp_libc_free(void*ptr)
{
    free(ptr);
}

static bool_t allocator_used=FALSE;

static OrtpMemoryFunctions ortp_allocator = {
    ortp_libc_malloc,
    ortp_libc_realloc,
    ortp_libc_free
};

void ortp_set_memory_functions(OrtpMemoryFunctions *functions)
{
    if (allocator_used){
        printf("ortp_set_memory_functions() must be called before "
        "first use of ortp_malloc or ortp_realloc" "\n");
        return;
    }
    ortp_allocator=*functions;
}

void* ortp_malloc(size_t sz)
{
    allocator_used=TRUE;
    return ortp_allocator.malloc_fun(sz);
}

void* ortp_realloc(void *ptr, size_t sz)
{
    allocator_used=TRUE;
    return ortp_allocator.realloc_fun(ptr,sz);
}

void ortp_free(void* ptr)
{
    ortp_allocator.free_fun(ptr);
}

void * ortp_malloc0(size_t size)
{
    void *ptr=ortp_malloc(size);
    memset(ptr,0,size);
    return ptr;
}


