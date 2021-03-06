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


#include <base/msqueue.h>
#include <string.h>

MSQueue * ms_queue_new(){
	MSQueue *q=(MSQueue*)ms_new0(MSQueue,1);
	qinit(&q->q);
	return q;
}

void ms_queue_init(MSQueue *q){
	qinit(&q->q);
}

void ms_queue_destroy(MSQueue *q){
	flushq(&q->q,0);
	ms_free(q);
}

void ms_queue_flush(MSQueue *q){
	flushq(&q->q,0);
}


void ms_bufferizer_init(MSBufferizer *obj){
	qinit(&obj->q);
	obj->size=0;
}

MSBufferizer * ms_bufferizer_new(){
	MSBufferizer *obj=(MSBufferizer *)ms_new0(MSBufferizer,1);
	ms_bufferizer_init(obj);
	return obj;
}

void ms_bufferizer_put(MSBufferizer *obj, mblk_t *m){
	obj->size+=msgdsize(m);
	putq(&obj->q,m);
}

void ms_bufferizer_put_from_queue(MSBufferizer *obj, MSQueue *q){
	mblk_t *m;
	while((m=ms_queue_get(q))!=NULL){
		ms_bufferizer_put(obj,m);
	}
}

int ms_bufferizer_read(MSBufferizer *obj, uint8_t *data, int datalen){
	if (obj->size>=datalen){
		/*we can return something */
		int sz=0;
		int cplen;
		mblk_t *m=peekq(&obj->q);
		
		/* first store current meta information in the _q_stopper field the queue, just to reuse space*/
		mblk_meta_copy(m, &obj->q._q_stopper);
		while(sz<datalen){
			cplen=MIN(m->b_wptr-m->b_rptr,datalen-sz);
			if (data) memcpy(data+sz,m->b_rptr,cplen);
			sz+=cplen;
			m->b_rptr+=cplen;
			if (m->b_rptr==m->b_wptr){
				/* check cont */
				if (m->b_cont!=NULL) {
					m=m->b_cont;
				}
				else{
					mblk_t *remove=getq(&obj->q);
					freemsg(remove);
					m=peekq(&obj->q);
				}
			}
		}
		obj->size-=datalen;
		return datalen;
	}
	return 0;
}

void ms_bufferizer_fill_current_metas(MSBufferizer *obj, mblk_t *dest){
	mblk_t *source=&obj->q._q_stopper;
#if defined(ORTP_TIMESTAMP)
	dest->timestamp = source->timestamp;
#endif
//	dest->ttl_or_hl = source->ttl_or_hl;
}

void ms_bufferizer_skip_bytes(MSBufferizer *obj, int bytes){
	ms_bufferizer_read(obj, NULL, bytes);
}

void ms_bufferizer_flush(MSBufferizer *obj){
	obj->size=0;
	flushq(&obj->q,0);
}

void ms_bufferizer_uninit(MSBufferizer *obj){
	flushq(&obj->q,0);
}

void ms_bufferizer_destroy(MSBufferizer *obj){
	ms_bufferizer_uninit(obj);
	ms_free(obj);
}
