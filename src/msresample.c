#include <stdio.h>
#include <string.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>


typedef struct
{
    struct SwrContext *swr_ctx;
    char **dst_data;
    int src_rate;
    int dst_rate;
    int src_ch_layout;
    int dst_ch_layout;
    int src_nb_channels;
    int dst_nb_channels;
    int max_dst_nb_samples;
    int src_sample_fmt;
    int dst_sample_fmt;
}ResampleData;


static int resample_context_init(ResampleData *d)
{
    int ret = -1;
    struct SwrContext *swr_ctx = NULL;

    if ((swr_ctx = swr_alloc()) == NULL)
    {
        printf("swr_alloc failed.\n");
        return -1;
    }

    /* set options */
    // 将resample信息写入resample上下文
    av_opt_set_int(swr_ctx, "in_channel_layout",    d->src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       d->src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", d->src_sample_fmt, 0);

    av_opt_set_int(swr_ctx, "out_channel_layout",    d->dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       d->dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", d->dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0)
    {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return -1;
    }

    d->swr_ctx = swr_ctx;
    return 0;
}




void resample_init(struct _MSFilter *f)
{
    ResampleData *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(ResampleData, 1);
    memset(d, 0, sizeof(ResampleData));
    d->src_rate = 8000;
    d->dst_rate = 44100;
    d->src_nb_channels = 1;
    d->dst_nb_channels = 1;
    d->src_ch_layout = av_get_default_channel_layout(d->src_nb_channels);
    d->dst_ch_layout = av_get_default_channel_layout(d->dst_nb_channels);
    d->src_sample_fmt = AV_SAMPLE_FMT_FLTP;
    d->dst_sample_fmt = AV_SAMPLE_FMT_FLTP;
    f->data = (void *)d;
}

void resample_preprocess(struct _MSFilter *f)
{
    ResampleData *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    resample_context_init(d);
}

void resample_process(struct _MSFilter *f)
{
    ResampleData *d = NULL;
    mblk_t *im = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        int len = im->b_wptr - im->b_rptr;
        int ret = -1;
        mblk_t *om = NULL;
        int src_rate = d->src_rate;
        int dst_rate = d->dst_rate;
        int src_ch_layout = d->src_ch_layout;
        int dst_ch_layout = d->dst_ch_layout;
        int src_nb_channels = d->src_nb_channels;
        int dst_nb_channels = d->dst_nb_channels;
        enum AVSampleFormat src_sample_fmt = d->src_sample_fmt;
        enum AVSampleFormat dst_sample_fmt = d->dst_sample_fmt;
        int src_nb_samples = len / av_get_bytes_per_sample(src_sample_fmt);
        int dst_nb_samples = 0;
        int dst_linesize = 0;
        int dst_bufsize = 0;

        dst_nb_samples = av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
        if (d->dst_data == NULL)
        {
            ret = av_samples_alloc_array_and_samples((uint8_t ***)&d->dst_data,
                &dst_linesize, dst_nb_channels, dst_nb_samples, dst_sample_fmt, 0);
        }
        
        // 重采样操作        
        ret = swr_convert(d->swr_ctx, (uint8_t **)d->dst_data, dst_nb_samples, (const uint8_t **)&im->b_rptr, src_nb_samples);
        if (ret < 0)
        {
            fprintf(stderr, "Error while converting\n");
            return;
        }
        
        dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, ret, dst_sample_fmt, 1);
        if (dst_bufsize < 0)
        {
            fprintf(stderr, "Could not get sample buffer size\n");
            return;
        }

        om = allocb(dst_bufsize, 0);
        memcpy(om->b_wptr, d->dst_data[0], dst_bufsize);
        om->b_wptr += dst_bufsize;
        ms_queue_put(f->outputs[0], om);

        freemsg(im);
    }
}

void resample_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static int resample_context_uninit(ResampleData *d)
{
    if (d->dst_data)
    {
        av_freep(d->dst_data);
    }

    if (d->swr_ctx != NULL)
    {
        swr_free(&d->swr_ctx);
    }

    return 0;
}

void resample_uninit(struct _MSFilter *f)
{
    ResampleData *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    resample_context_uninit(d);
    ms_free(d);
}

static int resample_set_sr(MSFilter *f, void *arg)
{
    ResampleData *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->src_rate = *((int *)arg);
    printf("%s : d->src_sample_rate = [%d]\n", __func__, d->src_rate);
    return 0;
}

static int resample_set_channels(MSFilter *f, void *arg)
{
    ResampleData *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->src_nb_channels = *((int *)arg);
    d->src_ch_layout = av_get_default_channel_layout(d->src_nb_channels);
    printf("%s : d->src_nb_channels = [%d]\n", __func__, d->src_nb_channels);
    return 0;
}

static int resample_set_sf(MSFilter *f, void *arg)
{
    ResampleData *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->src_sample_fmt = *((int *)arg);
    return 0;
}

static int resample_set_output_sr(MSFilter *f, void *arg)
{
    ResampleData *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->dst_rate = *((int *)arg);
    printf("%s : d->dst_sample_rate = [%d]\n", __func__, d->dst_rate);
    return 0;
}

static int resample_set_output_channels(MSFilter *f, void *arg)
{
    ResampleData *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->dst_nb_channels = *((int *)arg);
    d->dst_ch_layout = av_get_default_channel_layout(d->src_nb_channels);
    printf("%s : d->dst_channels = [%d]\n", __func__, d->dst_nb_channels);
    return 0;
}

static int resample_set_output_sf(MSFilter *f, void *arg)
{
    ResampleData *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ResampleData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->dst_sample_fmt = *((int *)arg);
    return 0;
}



MSFilterMethod resample_methods[] = {
    {MS_SET_SAMPLE_RATE, resample_set_sr},
    {MS_SET_CHANNELS,    resample_set_channels},
    {MS_SET_SAMPLE_FMT, resample_set_sf},
    {MS_SET_OUTPUT_SAMPLE_RATE, resample_set_output_sr},
    {MS_SET_OUTPUT_CHANNELS,    resample_set_output_channels},
    {MS_SET_OUTPUT_SAMPLE_FMT, resample_set_output_sf},
    {-1, NULL},
};


MSFilterDesc ms_resample_desc = {
    .id = MS_RESAMPLE_ID,
    .name = "AudioResample",
    .text = "audio resample",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 1,
    .noutputs = 1,
    .init = resample_init,
    .preprocess = resample_preprocess,
    .process = resample_process,
    .postprocess = resample_postprocess,
    .uninit = resample_uninit,
    .methods = resample_methods,
};


