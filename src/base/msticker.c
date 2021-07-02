#include <stdint.h>
#include <unistd.h>
#include <base/mscommon.h>
#include <base/msticker.h>
#include <base/msfilter.h>


#define TICKER_INTERVAL 10


static void run_graph(MSFilter *f, MSTicker *s)
{
    if (f->last_tick != s->ticks )
    {
        f->last_tick = s->ticks;
        f->desc->process(f);
    }
}

static void run_graphs(MSTicker *s, bctbx_list_t *execution_list)
{
    bctbx_list_t *it;
    for(it = execution_list; it != NULL; it = it->next)
    {
        run_graph((MSFilter*)it->data, s);
    }
}


/*the ticker thread function that executes the filters */
void * ms_ticker_run(void *arg)
{
    MSTicker *s = (MSTicker*)arg;
    s->thread_id = (unsigned long)pthread_self();
    s->ticks = 1;

    while(s->run)
    {
        s->ticks++;
        /*Step 1: run the graphs*/
        run_graphs(s, s->execution_list);

        /*Step 2: wait for next tick*/
//        usleep(10*1000);
    }
    printf("%s thread exiting\n", s->name);

    pthread_exit(NULL);
    s->thread_id = 0;
    return NULL;
}





static void ms_ticker_start(MSTicker *s)
{
    s->run=TRUE;
    pthread_create(&s->thread, NULL, ms_ticker_run, s);
}

static void ms_ticker_init(MSTicker *ticker)
{
//  ms_mutex_init(&ticker->lock,NULL);
//  ms_mutex_init(&ticker->cur_time_lock, NULL);
    ticker->execution_list = NULL;
//  ticker->task_list=NULL;
    ticker->ticks = 1;
    ticker->time = 0;
    ticker->interval = TICKER_INTERVAL;
    ticker->run = FALSE;
    ticker->exec_id = 0;
//  ticker->get_cur_time_ptr=&get_cur_time_ms;
//  ticker->get_cur_time_data=NULL;
    ticker->name = "MSTicker";
//    ticker->av_load=0;
//    ticker->prio=params->prio;
//    ticker->wait_next_tick=wait_next_tick;
//    ticker->wait_next_tick_data=ticker;
//    ticker->late_event.lateMs = 0;
//    ticker->late_event.time = 0;
//    ticker->late_event.current_late_ms = 0;
    ms_ticker_start(ticker);
}





MSTicker *ms_ticker_new()
{
    MSTicker *obj = (MSTicker *)ms_new0(MSTicker,1);
    ms_ticker_init(obj);
    return obj;
}

static void ms_ticker_stop(MSTicker *s)
{
    s->run = FALSE;
    if(s->thread) pthread_join(s->thread, NULL);
}


static void ms_ticker_uninit(MSTicker *ticker)
{
    ms_ticker_stop(ticker);
}

void ms_ticker_destroy(MSTicker *ticker)
{
    ms_ticker_uninit(ticker);
    ms_free(ticker);
}


