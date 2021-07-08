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
//#define FILTERS_DESCR "[in0]scale=w=768:h=432[v1];[v1][in1]xstack=inputs=2:layout=0_0|0_h0[vv];[vv]scale=w=1280:h=720[out]"
#define FILTERS_DESCR "[in0]scale=w=768:h=432[v1];[v1][in1]hstack=inputs=2[vv];[vv]scale=w=1280:h=720[out]"


typedef struct VideoMixer
{
    AVFilterGraph *filter_graph;
    AVFilter *buffersrc[MAX_STREAM_NUM];
    AVFilterContext *buffersrc_ctx[MAX_STREAM_NUM];
    AVFilterInOut *outputs[MAX_STREAM_NUM];
    AVFilter *buffersink;
    AVFilterContext *buffersink_ctx;
    AVFilterInOut *inputs;
    AVFrame *frame;
    AVFrame *filt_frame;

    int input_stream_count;
    int input_width[MAX_STREAM_NUM];
    int input_height[MAX_STREAM_NUM];
    int input_pix_fmt[MAX_STREAM_NUM];
    int output_width;
    int output_height;
    int output_pix_fmt;
    FILE *fp;
}VideoMixer;




void vmix_init(struct _MSFilter *f)
{
    VideoMixer *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(VideoMixer, 1);
    d->frame = av_frame_alloc();
    d->filt_frame = av_frame_alloc();
    d->fp = fopen("video.yuv", "wb");
    f->data = (void *)d;
}



static int init_filters(const char *filters_descr, VideoMixer *video_mix)
{
    int i = 0;
    int ret = -1;
    AVFilterInOut *outputs = NULL;
    enum AVPixelFormat pix_fmts[2] = { AV_PIX_FMT_NONE, AV_PIX_FMT_NONE };

    for (i = 0; i < video_mix->input_stream_count; i++)
    {
        video_mix->buffersrc[i] = avfilter_get_by_name("buffer");
        video_mix->outputs[i] = avfilter_inout_alloc();
        if (video_mix->buffersrc[i] == NULL || video_mix->outputs[i] == NULL)
        {
            printf("init buffersrc failed.\n");
            exit(0);
        }
    }

    video_mix->buffersink = avfilter_get_by_name("buffersink");
    video_mix->inputs = avfilter_inout_alloc();
    if (video_mix->buffersink == NULL || video_mix->inputs == NULL)
    {
        printf("init buffersink failed.\n");
        exit(0);
    }

    video_mix->filter_graph = avfilter_graph_alloc();
    if (video_mix->filter_graph == NULL)
    {
        printf("avfilter_graph_alloc() failed.\n");
        exit(0);
    }


    for (i = 0; i < video_mix->input_stream_count; i++)
    {
        char name[16] = {0};
        char args[512] = {0};
        snprintf(name, sizeof(name), "in%d", i);
        snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
                 video_mix->input_width[i], video_mix->input_height[i], video_mix->input_pix_fmt[i], 1, 90000);
        printf("args = [%s]\n", args);

        ret = avfilter_graph_create_filter(&video_mix->buffersrc_ctx[i], video_mix->buffersrc[i], name, args, NULL, video_mix->filter_graph);
        if (ret < 0)
        {
            printf("avfilter_graph_create_filter() abuffersrc[%d] failed : [%s]\n", i, av_err2str(ret));
            exit(0);
        }

        video_mix->outputs[i]->name       = av_strdup(name);
        video_mix->outputs[i]->filter_ctx = video_mix->buffersrc_ctx[i];
        video_mix->outputs[i]->pad_idx    = 0;
        video_mix->outputs[i]->next       = NULL;
        if (outputs == NULL)    outputs = video_mix->outputs[i];
        else                    outputs->next = video_mix->outputs[i];
    }

    ret = avfilter_graph_create_filter(&video_mix->buffersink_ctx, video_mix->buffersink, "out", NULL, NULL, video_mix->filter_graph);
    if (ret < 0)
    {
        printf("avfilter_graph_create_filter() failed : [%s]\n", av_err2str(ret));
        exit(0);
    }

    pix_fmts[0] = video_mix->output_pix_fmt;
    ret = av_opt_set_int_list(video_mix->buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        printf("av_opt_set_int_list() pix_fmts failed : [%s]\n", av_err2str(ret));
        exit(0);
    }


    video_mix->inputs->name       = av_strdup("out");
    video_mix->inputs->filter_ctx = video_mix->buffersink_ctx;
    video_mix->inputs->pad_idx    = 0;
    video_mix->inputs->next       = NULL;


    ret = avfilter_graph_parse_ptr(video_mix->filter_graph, filters_descr, &video_mix->inputs, &outputs, NULL);
    if (ret < 0)
    {
        printf("avfilter_graph_parse_ptr() failed : [%s]\n", av_err2str(ret));
        exit(0);
    }
    ret = avfilter_graph_config(video_mix->filter_graph, NULL);
    if (ret < 0)
    {
        printf("avfilter_graph_config() failed : [%s]\n", av_err2str(ret));
        exit(0);
    }


    for (i = 0; i < video_mix->filter_graph->nb_filters; i++)
    {
        printf("[%d] --> filter name = [%s]\n", i, video_mix->filter_graph->filters[i]->name);
    }

    return 0;
}




void vmix_preprocess(struct _MSFilter *f)
{
    int i = 0;
    VideoMixer *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }


    for (i = 0; i < d->input_stream_count; i++)
    {
        printf("%s : (input %d) --> width = [%d]\n", __func__, i, d->input_width[i]);
        printf("%s : (input %d) --> height = [%d]\n", __func__, i, d->input_height[i]);
        printf("%s : (input %d) --> pix_fmt = [%d]\n", __func__, i, d->input_pix_fmt[i]);
    }
    printf("%s : (output) --> width = [%d]\n", __func__, d->output_width);
    printf("%s : (output) --> height = [%d]\n", __func__, d->output_height);
    printf("%s : (output) --> pix_fmt = [%d]\n", __func__, d->output_pix_fmt);

    init_filters(FILTERS_DESCR, d);
}


void vmix_process(struct _MSFilter *f)
{
    int i, ret = -1;
    VideoMixer *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    for (i = 0; i < d->input_stream_count; i++)
    {
        while ((im = ms_queue_get(f->inputs[i])) != NULL)
        {
            int frame_size = d->input_width[i] * d->input_height[i];
            d->frame->pts = mblk_get_timestamp_info(im);
            d->frame->data[0] = im->b_rptr;
            d->frame->data[1] = im->b_rptr + frame_size;
            d->frame->data[2] = im->b_rptr + frame_size * 5 / 4;
            d->frame->linesize[0] = d->input_width[i];
            d->frame->linesize[1] = d->input_width[i]/2;
            d->frame->linesize[2] = d->input_width[i]/2;


            d->frame->format = d->input_pix_fmt[i];
            d->frame->width = d->input_width[i];
            d->frame->height = d->input_height[i];

            ret = av_buffersrc_add_frame_flags(d->buffersrc_ctx[i], d->frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0)
            {
                printf("av_buffersrc_add_frame_flags() failed : [%s]\n", av_err2str(ret));
            }

            freemsg(im);
        }
    }



    while (1)
    {
        int y,u,v;
        ret = av_buffersink_get_frame(d->buffersink_ctx, d->filt_frame);
//        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)   break;
        if (ret == AVERROR(EAGAIN))   break;
        if (ret == AVERROR_EOF)   ms_filter_notify_no_arg(f, 0);

        if (ret < 0)
        {
            printf("av_buffersink_get_frame() failed : [%s]\n", av_err2str(ret));
            ms_filter_notify_no_arg(f, 0);
            return;
        }

        om = allocb(d->filt_frame->width * d->filt_frame->height * 3 / 2, 0);

        for (y = 0; y < d->filt_frame->height; y++)
        {
//            fwrite(d->filt_frame->data[0]+y*d->filt_frame->linesize[0], 1, d->filt_frame->width, d->fp);
              memcpy(om->b_wptr+y*d->filt_frame->width, d->filt_frame->data[0]+y*d->filt_frame->linesize[0], d->filt_frame->width);
        }
        om->b_wptr += d->filt_frame->width * d->filt_frame->height;

        for (u = 0; u < d->filt_frame->height / 2; u++)
        {
//            fwrite(d->filt_frame->data[1]+u*d->filt_frame->linesize[1], 1, d->filt_frame->width/2, d->fp);
            memcpy(om->b_wptr+u*d->filt_frame->width/2, d->filt_frame->data[1]+u*d->filt_frame->linesize[1], d->filt_frame->width/2);
        }
        om->b_wptr += d->filt_frame->width * d->filt_frame->height / 4;

        for (v = 0; v < d->filt_frame->height / 2; v++)
        {
//            fwrite(d->filt_frame->data[2]+v*d->filt_frame->linesize[2], 1, d->filt_frame->width/2, d->fp);
            memcpy(om->b_wptr+v*d->filt_frame->width/2, d->filt_frame->data[2]+v*d->filt_frame->linesize[2], d->filt_frame->width/2);
        }
        om->b_wptr += d->filt_frame->width * d->filt_frame->height / 4;
        mblk_set_timestamp_info(om, d->filt_frame->pts);
        printf("%s : d->filt_frame->pts = [%d]\n", __func__, d->filt_frame->pts);

        ms_queue_put(f->outputs[0], om);
        av_frame_unref(d->filt_frame);
    }
}

void vmix_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}



void vmix_uninit(struct _MSFilter *f)
{
    VideoMixer *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }






    ms_free(d);
}



static int vmix_set_width(MSFilter *f, void *arg)
{
    VideoMixer *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->output_width = *((int *)arg);
    return 0;
}

static int vmix_set_height(MSFilter *f, void *arg)
{
    VideoMixer *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->output_height = *((int *)arg);
    return 0;
}

static int vmix_set_pix_fmt(MSFilter *f, void *arg)
{
    VideoMixer *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->output_pix_fmt = *((int *)arg);
    return 0;
}

static int vmix_set_info(MSFilter *f, void *arg)
{
    int ret = -1;
    char *info = NULL;
    char *ptr = NULL;
    char type[32] = {0};
    int data = 0;
    int w_count = 0;
    int h_count = 0;
    int p_count = 0;
    VideoMixer *d = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (VideoMixer *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    info = (char *)arg;

    do
    {
        if (w_count >= MAX_STREAM_NUM && h_count >= MAX_STREAM_NUM && p_count >= MAX_STREAM_NUM)
        {
            printf("too many parameter.\n");
            memset(d, 0, sizeof(VideoMixer));
            return -1;
        }

        memset(type, 0, sizeof(type));
        ret = sscanf(info, "%[^=]=%d", type, &data);
        if (ret != 2)
        {
            printf("Parse amix info failed.\n");
            memset(d, 0, sizeof(VideoMixer));
            return -1;
        }

        if (strcmp(type, "inputs") == 0)
        {
            d->input_stream_count = data;
        }
        else if (strcmp(type, "width") == 0)
        {
            d->input_width[w_count++] = data;
        }
        else if (strcmp(type, "height") == 0)
        {
            d->input_height[h_count++] = data;
        }
        else if (strcmp(type, "pix_fmt") == 0)
        {
            d->input_pix_fmt[p_count++] = data;
        }

        ptr = strchr(info, ':');
        info = ptr ? ptr+1 : NULL;
    }while (ptr != NULL);
    return 0;
}


MSFilterMethod vmix_methods[] = {
    {MS_SET_OUTPUT_WIDTH, vmix_set_width},
    {MS_SET_OUTPUT_HEIGTH,    vmix_set_height},
    {MS_SET_OUTPUT_PIX_FMT,  vmix_set_pix_fmt},
    {MS_SET_VMIX_INFO,   vmix_set_info},
    {-1, NULL},
};


MSFilterDesc ms_vmix_desc = {
    .id = MS_VMIX_ID,
    .name = "Vmix",
    .text = "video mixer",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 2,
    .noutputs = 1,
    .init = vmix_init,
    .preprocess = vmix_preprocess,
    .process = vmix_process,
    .postprocess = vmix_postprocess,
    .uninit = vmix_uninit,
    .methods = vmix_methods,
};


