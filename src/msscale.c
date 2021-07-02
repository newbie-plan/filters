#include <stdio.h>
#include <string.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct Scale
{
    struct SwsContext *sws_ctx;
    int src_width;
    int src_height;
    int src_pix_fmt;
    int dst_width;
    int dst_height;
    int dst_pix_fmt;
    FILE *fp;
}Scale;


static char *scale_name = "msscale.scale";


static int scale_context_init(Scale *d)
{
    int src_width = d->src_width;
    int src_height = d->src_height;
    int src_pix_fmt = d->src_pix_fmt;
    int dst_width = d->dst_width;
    int dst_height = d->dst_height;
    int dst_pix_fmt = d->dst_pix_fmt;

    struct SwsContext *c = sws_getContext(src_width, src_height, src_pix_fmt,
                                   dst_width, dst_height, dst_pix_fmt,
                                   SWS_BICUBIC,
                                   NULL, NULL, NULL);
    d->sws_ctx = c;
    return 0;
}


void scale_init(struct _MSFilter *f)
{
    Scale *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(Scale, 1);
    memset(d, 0, sizeof(Scale));
    d->src_width = 1920;
    d->src_height = 1920;
    d->src_pix_fmt = AV_PIX_FMT_YUV420P;
    d->dst_width = 1920;
    d->dst_height = 1920;
    d->dst_pix_fmt = AV_PIX_FMT_YUV420P;
    d->fp = fopen("scale.yuv", "wb");
    f->data = (void *)d;
}


void scale_preprocess(struct _MSFilter *f)
{
    Scale *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    scale_context_init(d);
}


void scale_process(struct _MSFilter *f)
{
    int ret = -1;
    Scale *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;
    uint8_t *src_slice[3] = {0};
    int src_stride[3] = {0};
    uint8_t *dst_slice[3] = {0};
    int dst_stride[3] = {0};
    int src_frame_size = 0;
    int dst_frame_size = 0;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    src_frame_size = d->src_width * d->src_height;
    dst_frame_size = d->dst_width * d->dst_height;

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        int timestamp = mblk_get_timestamp_info(im);

        memset(src_slice, 0, sizeof(src_slice));
        memset(src_stride, 0, sizeof(src_stride));
        memset(dst_slice, 0, sizeof(dst_slice));
        memset(dst_stride, 0, sizeof(dst_stride));

        src_slice[0] = im->b_rptr;
        src_slice[1] = im->b_rptr + src_frame_size;
        src_slice[2] = im->b_rptr + src_frame_size * 5 / 4;
        src_stride[0] = d->src_width;
        src_stride[1] = d->src_width / 2;
        src_stride[2] = d->src_width / 2;

        om = allocb(dst_frame_size * 3 / 2, 0);
        dst_slice[0] = om->b_wptr;
        dst_slice[1] = om->b_wptr + dst_frame_size;
        dst_slice[2] = om->b_wptr + dst_frame_size * 5 / 4;
        dst_stride[0] = d->dst_width;
        dst_stride[1] = d->dst_width / 2;
        dst_stride[2] = d->dst_width / 2;

        ret = sws_scale(d->sws_ctx, (const uint8_t * const*)src_slice, (const int *)src_stride, 0, d->src_height, (uint8_t * const*)dst_slice, (const int *)dst_stride);
        if (ret < 0)
        {
            printf("(%s) %s : sws_scale failed [%s]\n", scale_name, __func__, av_err2str(ret));
        }
        om->b_wptr += dst_frame_size * 3 / 2;
        mblk_set_timestamp_info(om, timestamp);
        ms_queue_put(f->outputs[0], om);

        freemsg(im);
    }
}


void scale_postprocess(struct _MSFilter *f)
{

}


static int scale_context_uninit(Scale *d)
{
    if (d->sws_ctx)
    {
        sws_freeContext(d->sws_ctx);
    }

    return 0;
}

void scale_uninit(struct _MSFilter *f)
{
    Scale *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    scale_context_uninit(d);
    ms_free(d);
}



static int scale_set_width(MSFilter *f, void *arg)
{
    Scale *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->src_width = *((int *)arg);
    printf("(%s) %s : src_width = [%d]\n", scale_name, __func__, d->src_width);
    return 0;
}

static int scale_set_height(MSFilter *f, void *arg)
{
    Scale *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->src_height = *((int *)arg);
    printf("(%s) %s : src_width = [%d]\n", scale_name, __func__, d->src_height);
    return 0;

}

static int scale_set_pf(MSFilter *f, void *arg)
{
    Scale *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->src_pix_fmt = *((int *)arg);
    printf("(%s) %s : src_width = [%d]\n", scale_name, __func__, d->src_pix_fmt);
    return 0;

}

static int scale_set_output_width(MSFilter *f, void *arg)
{
    Scale *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->dst_width = *((int *)arg);
    printf("(%s) %s : src_width = [%d]\n", scale_name, __func__, d->dst_width);
    return 0;

}

static int scale_set_output_height(MSFilter *f, void *arg)
{
    Scale *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->dst_height = *((int *)arg);
    printf("(%s) %s : src_width = [%d]\n", scale_name, __func__, d->dst_height);
    return 0;

}


static int scale_set_output_pf(MSFilter *f, void *arg)
{
    Scale *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Scale *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->dst_pix_fmt = *((int *)arg);
    printf("(%s) %s : src_width = [%d]\n", scale_name, __func__, d->dst_pix_fmt);
    return 0;

}


MSFilterMethod scale_methods[] = {
    {MS_SET_WIDTH, scale_set_width},
    {MS_SET_HEIGTH, scale_set_height},
    {MS_SET_PIX_FMT, scale_set_pf},
    {MS_SET_OUTPUT_WIDTH, scale_set_output_width},
    {MS_SET_OUTPUT_HEIGTH, scale_set_output_height},
    {MS_SET_OUTPUT_PIX_FMT, scale_set_output_pf},
    {-1, NULL},
};

MSFilterDesc ms_scale_desc = {
    .id = MS_SCALE_ID,
    .name = "Scale",
    .text = "video scale",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 1,
    .noutputs = 1,
    .init = scale_init,
    .preprocess = scale_preprocess,
    .process = scale_process,
    .postprocess = scale_postprocess,
    .uninit = scale_uninit,
    .methods = scale_methods,
    .flags = MS_FILTER_IS_ENABLED
};

