#ifndef __MS_TICKER_H__
#define __MS_TICKET_H__
#include <pthread.h>
#include <bctoolbox/bctoolbox.h>



typedef struct _MSTicker
{
//    ms_mutex_t lock; /*main lock protecting the filter execution list */
//    ms_cond_t cond;
    bctbx_list_t *execution_list;     /* the list of source filters to be executed.*/
//    bctbx_list_t *task_list; /* list of tasks (see ms_filter_postpone_task())*/
    pthread_t thread;   /* the thread ressource*/
    int interval; /* in miliseconds*/
    int exec_id;
    uint32_t ticks;
    uint64_t time;  /* a time since the start of the ticker expressed in milisec*/
    uint64_t orig; /* a relative time to take in account difference between time base given by consecutive get_cur_time_ptr() functions.*/
//    MSTickerTimeFunc get_cur_time_ptr;
//    void *get_cur_time_data;
//    ms_mutex_t cur_time_lock; /*mutex protecting the get_cur_time_ptr/get_cur_time_data which can be changed at any time*/
    char *name;
//    double av_load;   /*average load of the ticker */
//    MSTickerPrio prio;
//    MSTickerTickFunc wait_next_tick;
//    void *wait_next_tick_data;
//    MSTickerLateEvent late_event;
    unsigned long thread_id;
    bool_t run;       /* flag to indicate whether the ticker must be run or not */
}MSTicker;


MSTicker *ms_ticker_new();
void ms_ticker_destroy(MSTicker *ticker);


#endif


