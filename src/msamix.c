#include <stdio.h>
#include <string.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libavutil/frame.h>



#define MAX_STREAM_NUM 2
#define FILTERS_DESCR "[in0]volume=5dB[a0];[in1]volume=10dB[a1];[a0][a1]amix=inputs=2:duration=first:dropout_transition=3[out]"
//#define FILTERS_DESCR "[in0]amix=inputs=1:duration=first:dropout_transition=3[out]"




typedef struct AudioMixer
{
    AVFilterGraph *filter_graph;
    AVFilter *abuffersrc[MAX_STREAM_NUM];
    AVFilterContext *abuffersrc_ctx[MAX_STREAM_NUM];
    AVFilterInOut *outputs[MAX_STREAM_NUM];
    AVFilter *abuffersink;
    AVFilterContext *abuffersink_ctx;
    AVFilterInOut *inputs;
    AVFrame *frame;
    AVFrame *filt_frame;
    int input_stream_count;
    int input_sample_rate[MAX_STREAM_NUM];
    int input_sample_fmt[MAX_STREAM_NUM];
    int input_channels[MAX_STREAM_NUM];
    int output_sample_rate;
    int output_sample_fmt;
    int output_channels;
}AudioMixer;








void amix_init(struct _MSFilter *f)
{
    AudioMixer *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(AudioMixer, 1);
    memset(d, 0, sizeof(AudioMixer));

    d->frame = av_frame_alloc();
    d->filt_frame = av_frame_alloc();
    f->data = (void *)d;
}



static int init_filters(const char *filters_descr, AudioMixer *audio_mix)
{
    int i = 0;
    int ret = -1;
    AVFilterInOut *outputs = NULL;
    int sample_rate[2] = {-1, 0};
    int channel_layouts[2] = {-1, 0};
    enum AVSampleFormat sample_fmts[2] = {-1, -1};      /*以0结尾还是以-1结尾,需要再研究下*/
//    static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, -1 };

    for (i = 0; i < audio_mix->input_stream_count; i++)
    {
        audio_mix->abuffersrc[i] = avfilter_get_by_name("abuffer");
        audio_mix->outputs[i] = avfilter_inout_alloc();
        if (audio_mix->abuffersrc[i] == NULL || audio_mix->outputs[i] == NULL)
        {
            printf("init buffersrc failed.\n");
            exit(0);
        }
    }

    audio_mix->abuffersink = avfilter_get_by_name("abuffersink");
    audio_mix->inputs = avfilter_inout_alloc();
    if (audio_mix->abuffersink == NULL || audio_mix->inputs == NULL)
    {
        printf("init buffersink failed.\n");
        exit(0);
    }

    audio_mix->filter_graph = avfilter_graph_alloc();
    if (audio_mix->filter_graph == NULL)
    {
        printf("avfilter_graph_alloc() failed.\n");
        exit(0);
    }


    for (i = 0; i < audio_mix->input_stream_count; i++)
    {
        char name[16] = {0};
        char args[512] = {0};
        snprintf(name, sizeof(name), "in%d", i);
        snprintf(args, sizeof(args), "sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
                 audio_mix->input_sample_rate[i], av_get_sample_fmt_name(audio_mix->input_sample_fmt[i]), av_get_default_channel_layout(audio_mix->input_channels[i]));
        printf("%s : args = [%s]\n", __func__, args);

        ret = avfilter_graph_create_filter(&audio_mix->abuffersrc_ctx[i], audio_mix->abuffersrc[i], name, args, NULL, audio_mix->filter_graph);
        if (ret < 0)
        {
            printf("avfilter_graph_create_filter() abuffersrc[%d] failed : [%s]\n", i, av_err2str(ret));
            exit(0);
        }

        audio_mix->outputs[i]->name       = av_strdup(name);
        audio_mix->outputs[i]->filter_ctx = audio_mix->abuffersrc_ctx[i];
        audio_mix->outputs[i]->pad_idx    = 0;
        audio_mix->outputs[i]->next       = NULL;
        if (outputs == NULL)    outputs = audio_mix->outputs[i];
        else                    outputs->next = audio_mix->outputs[i];
    }

    ret = avfilter_graph_create_filter(&audio_mix->abuffersink_ctx, audio_mix->abuffersink, "out", NULL, NULL, audio_mix->filter_graph);
    if (ret < 0)
    {
        printf("avfilter_graph_create_filter() failed : [%s]\n", av_err2str(ret));
        exit(0);
    }

    sample_rate[0] = audio_mix->output_sample_rate;
    channel_layouts[0] = av_get_default_channel_layout(audio_mix->output_channels);
    sample_fmts[0] = audio_mix->output_sample_fmt;
    ret = av_opt_set_int_list(audio_mix->abuffersink_ctx, "sample_fmts", sample_fmts, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("av_opt_set_int_list() sample_fmts failed : [%s]\n", av_err2str(ret));
        exit(0);
    }
    ret = av_opt_set_int_list(audio_mix->abuffersink_ctx, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("av_opt_set_int_list() channel_layouts failed : [%s]\n", av_err2str(ret));
        exit(0);
    }
    ret = av_opt_set_int_list(audio_mix->abuffersink_ctx, "sample_rates", sample_rate, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("av_opt_set_int_list() sample_rates failed : [%s]\n", av_err2str(ret));
        exit(0);
    }

    audio_mix->inputs->name       = av_strdup("out");
    audio_mix->inputs->filter_ctx = audio_mix->abuffersink_ctx;
    audio_mix->inputs->pad_idx    = 0;
    audio_mix->inputs->next       = NULL;


    ret = avfilter_graph_parse_ptr(audio_mix->filter_graph, filters_descr, &audio_mix->inputs, &outputs, NULL);
    if (ret < 0)
    {
        printf("avfilter_graph_parse_ptr() failed : [%s]\n", av_err2str(ret));
        exit(0);
    }
    ret = avfilter_graph_config(audio_mix->filter_graph, NULL);
    if (ret < 0)
    {
        printf("avfilter_graph_config() failed : [%s]\n", av_err2str(ret));
        exit(0);
    }


    for (i = 0; i < audio_mix->filter_graph->nb_filters; i++)
    {
        printf("[%d] --> filter name = [%s]\n", i, audio_mix->filter_graph->filters[i]->name);
    }

    return 0;
}


void amix_preprocess(struct _MSFilter *f)
{
    int i = 0;
    AudioMixer *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    for (i = 0; i < d->input_stream_count; i++)
    {
        printf("%s : (input %d) --> sample_rate = [%d]\n", __func__, i, d->input_sample_rate[i]);
        printf("%s : (input %d) --> channels = [%d]\n", __func__, i, d->input_channels[i]);
        printf("%s : (input %d) --> sample_fmt = [%d]\n", __func__, i, d->input_sample_fmt[i]);
    }
    printf("%s : (output) --> sample_rate = [%d]\n", __func__, d->output_sample_rate);
    printf("%s : (output) --> channels = [%d]\n", __func__, d->output_channels);
    printf("%s : (output) --> sample_fmt = [%d]\n", __func__, d->output_sample_fmt);

#if 0
    for (i = 0; i < d->input_stream_count; i++)
    {

        d->frame[i] = av_frame_alloc();
        if (frame[i] == NULL)
        {
            printf("av_frame_alloc() failed.\n");
            return;
        }
        d->frame[i]->nb_samples = FRAME_SIZE;
        d->frame[i]->format = AV_SAMPLE_FMT_S16;
        d->frame[i]->channel_layout = av_get_default_channel_layout(param.input_channels[i]);
        d->frame[i]->channels = param.input_channels[i];
        d->frame[i]->sample_rate = param.input_sample_rate[i];

        ret = av_frame_get_buffer(frame[i], 0);
        if (ret < 0)
        {
            printf("av_frame_get_buffer() failed : [%s]\n", av_err2str(ret));
            return -1;
        }
    }
#endif

    init_filters(FILTERS_DESCR, d);
}


void amix_process(struct _MSFilter *f)
{
    int i = 0, ret = -1;
    AudioMixer *d = NULL;
    AVFrame *frame = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    frame = d->frame;

    for (i = 0; i < d->input_stream_count; i++)
    {
        while ((im = ms_queue_get(f->inputs[i])) != NULL)
        {
            d->frame->data[0] = im->b_rptr;

            d->frame->nb_samples = (im->b_wptr - im->b_rptr) / av_get_bytes_per_sample(d->input_sample_fmt[i]);
            d->frame->format = d->input_sample_fmt[i];
            d->frame->channel_layout = av_get_default_channel_layout(d->input_channels[i]);
            d->frame->channels = d->input_channels[i];
            d->frame->sample_rate = d->input_sample_rate[i];
//            printf("%s : %d --> nb_samples = [%d] : sample_rate = [%d]\n", __func__, i, d->frame->nb_samples, d->frame->sample_rate);

#if 0
            ret = av_frame_get_buffer(d->frame, 0);
            if (ret < 0)
            {
                printf("av_frame_get_buffer() failed : [%s]\n", av_err2str(ret));
                freemsg(im);
                return;
            }
            memcpy(d->frame->data[0], im->b_rptr, im->b_wptr - im->b_rptr);
#endif

            ret = av_buffersrc_add_frame_flags(d->abuffersrc_ctx[i], d->frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0)
            {
                printf("av_buffersrc_add_frame_flags() failed : [%s]\n", av_err2str(ret));
            }

            freemsg(im);
        }
    }

    while (1)
    {
        int data_size = 0;
        ret = av_buffersink_get_frame(d->abuffersink_ctx, d->filt_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)   break;
        if (ret < 0)
        {
            printf("av_buffersink_get_frame() failed : [%s]\n", av_err2str(ret));
            return;
        }
        data_size = av_samples_get_buffer_size(NULL, d->filt_frame->channels, d->filt_frame->nb_samples, d->filt_frame->format, 1);
//        printf("%s ：nb_samples = [%d] : channels = [%d] : format = [%d] sample_rate = [%d] : data_size = [%d]\n", 
//            __func__, d->filt_frame->nb_samples,d->filt_frame->channels, d->filt_frame->format, d->filt_frame->sample_rate, data_size);
        om = allocb(data_size, 0);
        memcpy(om->b_wptr, d->filt_frame->data[0], data_size);
        om->b_wptr += data_size;
        ms_queue_put(f->outputs[0], om);
        av_frame_unref(d->filt_frame);
    }

    
}

void amix_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}



void amix_uninit(struct _MSFilter *f)
{
    AudioMixer *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }



    ms_free(d);
}

static int amix_set_sr(MSFilter *f, void *arg)
{
    AudioMixer *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->output_sample_rate = *((int *)arg);
    return 0;
}

static int amix_set_channels(MSFilter *f, void *arg)
{
    AudioMixer *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->output_channels = *((int *)arg);
    return 0;
}

static int amix_set_sf(MSFilter *f, void *arg)
{
    AudioMixer *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->output_sample_fmt = *((int *)arg);
    return 0;
}

static int amix_set_info(MSFilter *f, void *arg)
{
    int ret = -1;
    char *info = NULL;
    char *ptr = NULL;
    char type[32] = {0};
    int data = 0;
    int sr_count = 0;
    int sf_count = 0;
    int ch_count = 0;
    AudioMixer *d = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AudioMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    info = (char *)arg;

    do
    {
        if (sr_count >= MAX_STREAM_NUM && sf_count >= MAX_STREAM_NUM && ch_count >= MAX_STREAM_NUM)
        {
            printf("too many parameter.\n");
            memset(d, 0, sizeof(AudioMixer));
            return -1;
        }

//        printf("%s : info = [%s]\n", __func__, info);
        memset(type, 0, sizeof(type));
        ret = sscanf(info, "%[^=]=%d", type, &data);
        if (ret != 2)
        {
            printf("Parse amix info failed.\n");
            memset(d, 0, sizeof(AudioMixer));
            return -1;
        }

        if (strcmp(type, "inputs") == 0)
        {
            d->input_stream_count = data;
        }
        else if (strcmp(type, "sample_rate") == 0)
        {
            d->input_sample_rate[sr_count++] = data;
        }
        else if (strcmp(type, "channels") == 0)
        {
            d->input_channels[ch_count++] = data;
        }
        else if (strcmp(type, "sample_fmt") == 0)
        {
            d->input_sample_fmt[sf_count++] = data;
        }

        ptr = strchr(info, ':');
        info = ptr ? ptr+1 : NULL;
    }while (ptr != NULL);
    return 0;
}



MSFilterMethod amix_methods[] = {
    {MS_SET_OUTPUT_SAMPLE_RATE, amix_set_sr},
    {MS_SET_OUTPUT_CHANNELS,    amix_set_channels},
    {MS_SET_OUTPUT_SAMPLE_FMT,  amix_set_sf},
    {MS_SET_AMIX_INFO,   amix_set_info},
    {-1, NULL},
};



MSFilterDesc ms_amix_desc = {
    .id = MS_AMIX_ID,
    .name = "Amix",
    .text = "audio mixer",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 2,
    .noutputs = 1,
    .init = amix_init,
    .preprocess = amix_preprocess,
    .process = amix_process,
    .postprocess = amix_postprocess,
    .uninit = amix_uninit,
    .methods = amix_methods,
};


