#include <base/msfilter.h>


typedef struct _MSNotifyContext
{
    MSFilterNotifyFunc fn;
    void *ud;
    int synchronous;
}MSNotifyContext;


void ms_filter_destroy(MSFilter *f)
{
    if (f->desc->uninit!=NULL)
        f->desc->uninit(f);

    if (f->inputs!=NULL)    ms_free(f->inputs);
    if (f->outputs!=NULL)   ms_free(f->outputs);
//    ms_mutex_destroy(&f->lock);
    ms_filter_clear_notify_callback(f);
//    ms_filter_clean_pending_events(f);
    ms_free(f);
}


void ms_connection_helper_start(MSConnectionHelper *h)
{
    h->last.filter=0;
    h->last.pin=-1;
}
int ms_connection_helper_link(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin)
{
    int err=0;
    if (h->last.filter == NULL)
    {
        h->last.filter = f;
        h->last.pin = outpin;
    }
    else
    {
        err = ms_filter_link(h->last.filter, h->last.pin, f, inpin);
        if (err == 0)
        {
            h->last.filter = f;
            h->last.pin = outpin;
        }
    }
    return err;
}
int ms_connection_helper_unlink(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin)
{
    int err=0;
    if (h->last.filter == NULL)
    {
        h->last.filter = f;
        h->last.pin = outpin;
    }
    else
    {
        err = ms_filter_unlink(h->last.filter, h->last.pin, f, inpin);
        if (err == 0)
        {
            h->last.filter = f;
            h->last.pin = outpin;
        }
    }
    return err;
}

int ms_filter_link(MSFilter *f1, int pin1, MSFilter *f2, int pin2)
{
    MSQueue *q;
    printf("ms_filter_link: %s:%p,%i-->%s:%p,%i\n",f1->desc->name,f1,pin1,f2->desc->name,f2,pin2);
//    ms_return_val_if_fail(pin1<f1->desc->noutputs, -1);
//    ms_return_val_if_fail(pin2<f2->desc->ninputs, -1);
//    ms_return_val_if_fail(f1->outputs[pin1]==NULL,-1);
//    ms_return_val_if_fail(f2->inputs[pin2]==NULL,-1);

    q = ms_queue_new(f1, pin1, f2, pin2);
    f1->outputs[pin1] = q;
    f2->inputs[pin2] = q;
    return 0;
}
int ms_filter_unlink(MSFilter *f1, int pin1, MSFilter *f2, int pin2)
{
    MSQueue *q;
    printf("ms_filter_unlink: %s:%p,%i-->%s:%p,%i\n",f1 ? f1->desc->name : "!NULL!",f1,pin1,f2 ? f2->desc->name : "!NULL!",f2,pin2);
//    ms_return_val_if_fail(pin1<f1->desc->noutputs, -1);
//    ms_return_val_if_fail(pin2<f2->desc->ninputs, -1);
//    ms_return_val_if_fail(f1->outputs[pin1]!=NULL,-1);
//    ms_return_val_if_fail(f2->inputs[pin2]!=NULL,-1);
//    ms_return_val_if_fail(f1->outputs[pin1]==f2->inputs[pin2],-1);

    q = f1->outputs[pin1];
    f1->outputs[pin1] = f2->inputs[pin2] = 0;
    ms_queue_destroy(q);
    return 0;
}


int ms_filter_call_method(MSFilter *f, unsigned int id, void *arg)
{
    if (f)
    {
        int i;
        MSFilterMethod *methods = f->desc->methods;
        for(i = 0; methods != NULL && methods[i].method != NULL; i++)
        {
            if (methods[i].id == id)
            {
                return methods[i].method(f,arg);
            }
        }
    }
    else
    {
        printf("[%s] Ignoring call to filter method as the provided filter is NULL", __func__);
    }
    return -1;
}


static MSNotifyContext * ms_notify_context_new(MSFilterNotifyFunc fn, void *ud, bool_t synchronous)
{
    MSNotifyContext *ctx = ms_new0(MSNotifyContext,1);
    ctx->fn = fn;
    ctx->ud = ud;
    ctx->synchronous = synchronous;
    return ctx;
}


void ms_filter_add_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud, bool_t synchronous)
{
    f->notify_callbacks = bctbx_list_append(f->notify_callbacks, ms_notify_context_new(fn, ud, synchronous));
}

void ms_filter_set_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud)
{
    ms_filter_add_notify_callback(f,fn,ud,FALSE);
}

static void ms_notify_context_destroy(MSNotifyContext *obj)
{
    ms_free(obj);
}

void ms_filter_remove_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud)
{
    bctbx_list_t *elem;
    bctbx_list_t *found = NULL;
    for(elem = f->notify_callbacks; elem != NULL;elem = elem->next)
    {
        MSNotifyContext *ctx = (MSNotifyContext*)elem->data;
        if (ctx->fn == fn && ctx->ud == ud)
        {
            found = elem;
            break;
        }
    }
    if (found)
    {
        ms_notify_context_destroy((MSNotifyContext*)found->data);
        f->notify_callbacks = bctbx_list_erase_link(f->notify_callbacks, found);
    }
    else 
    {
        printf("ms_filter_remove_notify_callback(filter=%p): no registered callback with fn=%p and ud=%p",f,fn,ud);
    }
}

void ms_filter_clear_notify_callback(MSFilter *f)
{
    f->notify_callbacks = bctbx_list_free_with_data(f->notify_callbacks, (void (*)(void*))ms_notify_context_destroy);
}


static void ms_filter_invoke_callbacks(MSFilter **f, unsigned int id, void *arg)
{
    bctbx_list_t *elem;
    for (elem = (*f)->notify_callbacks; elem != NULL; elem = elem->next)
    {
        MSNotifyContext *ctx = (MSNotifyContext*)elem->data;

        ctx->fn(ctx->ud,*f,id,arg);

        if (*f == NULL) break; /*the filter was destroyed by a callback invocation*/
    }
}




void ms_filter_notify(MSFilter *f, unsigned int id, void *arg)
{
    if (f->notify_callbacks != NULL)
    {
        ms_filter_invoke_callbacks(&f, id, arg);
    }
}

void ms_filter_notify_no_arg(MSFilter *f, unsigned int id)
{
    ms_filter_notify(f, id, NULL);
}



