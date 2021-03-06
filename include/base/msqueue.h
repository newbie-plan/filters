/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#ifndef MSQUEUE_H
#define MSQUEUE_H
#include "ortp/str_utils.h"
#include "base/mscommon.h"

typedef struct _MSQueue
{
	queue_t q;
}MSQueue;


MSQueue * ms_queue_new();

static mblk_t *ms_queue_get(MSQueue *q){
	return getq(&q->q);
}

static void ms_queue_put(MSQueue *q, mblk_t *m){
	putq(&q->q,m);
	return;
}

static mblk_t * ms_queue_peek_last(MSQueue *q){
	return qlast(&q->q);
}

static mblk_t *ms_queue_peek_first(MSQueue *q){
	return qbegin(&q->q);
}

static mblk_t *ms_queue_next(MSQueue *q, mblk_t *m){
	return m->b_next;
}

static bool_t ms_queue_end(MSQueue *q, mblk_t *m){
	return qend(&q->q,m);
}

static void ms_queue_remove(MSQueue *q, mblk_t *m){
	remq(&q->q,m);
}

static bool_t ms_queue_empty(MSQueue *q){
	return qempty(&q->q);
}

#ifdef __cplusplus
extern "C"
{
#endif

/*yes these functions need to be public for plugins to work*/

/*init a queue on stack*/
void ms_queue_init(MSQueue *q);

void ms_queue_flush(MSQueue *q);

void ms_queue_destroy(MSQueue *q);


#define __mblk_set_flag(m,pos,bitval) \
	(m)->reserved2=(m->reserved2 & ~(1<<pos)) | ((!!bitval)<<pos) 
	
#define mblk_set_timestamp_info(m,ts) (m)->reserved1=(ts);
#define mblk_get_timestamp_info(m)    ((m)->reserved1)
#define mblk_set_marker_info(m,bit)   __mblk_set_flag(m,0,bit)
#define mblk_get_marker_info(m)	      ((m)->reserved2 & 0x1) /*bit 1*/

#define mblk_set_precious_flag(m,bit)    __mblk_set_flag(m,1,bit)  /*use to prevent mirroring for video*/
#define mblk_get_precious_flag(m)    (((m)->reserved2)>>1 & 0x1) /*bit 2 */

#define mblk_set_plc_flag(m,bit)    __mblk_set_flag(m,2,bit)  /*use to mark a plc generated block*/
#define mblk_get_plc_flag(m)    (((m)->reserved2)>>2 & 0x1) /*bit 3*/

#define mblk_set_cng_flag(m,bit)    __mblk_set_flag(m,3,bit)  /*use to mark a cng generated block*/
#define mblk_get_cng_flag(m)    (((m)->reserved2)>>3 & 0x1) /*bit 4*/

#define mblk_set_user_flag(m,bit)    __mblk_set_flag(m,7,bit)  /* to be used by extensions to mediastreamer2*/
#define mblk_get_user_flag(m)    (((m)->reserved2)>>7 & 0x1) /*bit 8*/

#define mblk_set_cseq(m,value) (m)->reserved2=(m)->reserved2| ((value&0xFFFF)<<16);	
#define mblk_get_cseq(m) ((m)->reserved2>>16)

#define HAVE_ms_bufferizer_fill_current_metas
	
struct _MSBufferizer{
	queue_t q;
	int size;
};

typedef struct _MSBufferizer MSBufferizer;

/*allocates and initialize */
MSBufferizer * ms_bufferizer_new(void);

/*initialize in memory */
void ms_bufferizer_init(MSBufferizer *obj);

void ms_bufferizer_put(MSBufferizer *obj, mblk_t *m);

/* put every mblk_t from q, into the bufferizer */
void ms_bufferizer_put_from_queue(MSBufferizer *obj, MSQueue *q);

/*read bytes from bufferizer object*/
int ms_bufferizer_read(MSBufferizer *obj, uint8_t *data, int datalen);

/*obtain current meta-information of the last read bytes (if any) and copy them into 'm'*/
void ms_bufferizer_fill_current_metas(MSBufferizer *obj, mblk_t *m);

/* returns the number of bytes available in the bufferizer*/
static int ms_bufferizer_get_avail(MSBufferizer *obj){
	return obj->size;
}

void ms_bufferizer_skip_bytes(MSBufferizer *obj, int bytes);

/* purge all data pending in the bufferizer */
void ms_bufferizer_flush(MSBufferizer *obj);

void ms_bufferizer_uninit(MSBufferizer *obj);

void ms_bufferizer_destroy(MSBufferizer *obj);

#ifdef __cplusplus
}
#endif

#endif
