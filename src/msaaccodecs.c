#include <stdio.h>
#include <string.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>


void aac_dec_init(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);


}

void aac_dec_preprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}


void aac_dec_process(struct _MSFilter *f)
{
    mblk_t *im = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        printf("[%02x %02x %02x %02x]\n\n", im->b_rptr[0], im->b_rptr[1], im->b_rptr[2], im->b_rptr[3]);
        freemsg(im);
    }
}

void aac_dec_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

void aac_dec_uninit(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}


MSFilterDesc ms_aac_dec_desc = {
    .id = MS_AAC_DEC_ID,
    .name = "AACDec",
    .text = "aac decoder",
    .category = MS_FILTER_DECODER,
    .enc_fmt = "AAC",
    .ninputs = 1,
    .noutputs = 1,
    .init = aac_dec_init,
    .preprocess = aac_dec_preprocess,
    .process = aac_dec_process,
    .postprocess = aac_dec_postprocess,
    .uninit = aac_dec_uninit,
    .methods = NULL,
};





typedef struct
{
    AVCodec *codec;
    AVCodecContext *codec_ctx;
    AVPacket *pkt;
    AVFrame *frame;
    int sample_rate;
    int channels;    
    int sample_fmt;
    int frame_size;
    int bit_rate;
    MSBufferizer encoder;
}AacEncoder;


static int encoder_init(AacEncoder *d)
{
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *cdc_ctx = NULL;

    if ((codec = avcodec_find_encoder(AV_CODEC_ID_AAC)) == NULL)
    {
        fprintf(stderr, "avcodec_find_encoder failed.\n");
        return -1;
    }

    if ((cdc_ctx = avcodec_alloc_context3(codec)) == NULL)
    {
        fprintf(stderr, "avcodec_alloc_context3 failed.\n");
        return -1;
    }

    cdc_ctx->bit_rate = d->bit_rate;
    cdc_ctx->sample_fmt = d->sample_fmt;
    cdc_ctx->sample_rate = d->sample_rate;
    cdc_ctx->channel_layout = av_get_default_channel_layout(d->channels);
    cdc_ctx->channels = d->channels;

    if ((ret = avcodec_open2(cdc_ctx, codec, NULL)) < 0)
    {
        fprintf(stderr, "avcodec_open2 failed.\n");
        return -1;
    }


    d->codec = codec;
    d->codec_ctx = cdc_ctx;
    d->pkt = av_packet_alloc();
    d->frame = av_frame_alloc();
    d->frame->nb_samples = cdc_ctx->frame_size;
    d->frame->format = cdc_ctx->sample_fmt;
    d->frame->channel_layout = cdc_ctx->channel_layout;
    if ((ret = av_frame_get_buffer(d->frame, 0)) < 0)
    {
        fprintf(stderr, "av_frame_get_buffer failed.\n");
        return -1;
    }
    return 0;
}


void aac_enc_init(struct _MSFilter *f)
{
    AacEncoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(AacEncoder, 1);
    d->sample_rate = 48000;
    d->channels = 1;
    d->sample_fmt = AV_SAMPLE_FMT_FLTP;
    d->frame_size = 1024;
    d->bit_rate = 192000;

    ms_bufferizer_init(&d->encoder);
    f->data = (void *)d;
}

void aac_enc_preprocess(struct _MSFilter *f)
{
    AacEncoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    encoder_init(d);
}


void aac_enc_process(struct _MSFilter *f)
{
    AacEncoder *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;
    int data_size = 0;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    data_size = av_samples_get_buffer_size(NULL, d->codec_ctx->channels, d->codec_ctx->frame_size, d->codec_ctx->sample_fmt, 1);

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        ms_bufferizer_put(&d->encoder, im);
        while (ms_bufferizer_get_avail(&d->encoder) >= data_size)
        {
            int ret = -1;
            ms_bufferizer_read(&d->encoder, d->frame->data[0], data_size);
            if ((ret = avcodec_send_frame(d->codec_ctx, d->frame)) < 0)
            {
                fprintf(stderr, "avcodec_send_frame failed.\n");
                return;
            }
            
            while ((ret = avcodec_receive_packet(d->codec_ctx, d->pkt)) >= 0)
            {
                om = allocb(d->pkt->size, 0);
                memcpy(om->b_wptr, d->pkt->data, d->pkt->size);
                om->b_wptr += d->pkt->size;
                ms_queue_put(f->outputs[0], om);

                av_packet_unref(d->pkt);
            }
        }
    }
}

void aac_enc_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}


static int encoder_uninit(AacEncoder *d)
{
    if (d->codec_ctx != NULL)
    {
        avcodec_free_context(&d->codec_ctx);
    }
    if (d->frame != NULL)
    {
        av_frame_free(&d->frame);
    }
    if (d->pkt != NULL)
    {
        av_packet_free(&d->pkt);
    }

    return 0;
}



void aac_enc_uninit(struct _MSFilter *f)
{
    AacEncoder *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    ms_bufferizer_flush(&d->encoder);
    encoder_uninit(d);
    ms_free(d);
}




static int aac_enc_get_sr(MSFilter *f, void *arg)
{
    AacEncoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_rate;
    return 0;
}

static int aac_enc_get_channels(MSFilter *f, void *arg)
{
    AacEncoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->channels;
    return 0;
}

static int aac_enc_get_sf(MSFilter *f, void *arg)
{
    AacEncoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_fmt;
    return 0;
}

static int aac_enc_get_frame_size(MSFilter *f, void *arg)
{
    AacEncoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->frame_size;
    return 0;
}


static int aac_enc_set_sr(MSFilter *f, void *arg)
{
    AacEncoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->sample_rate = *((int *)arg);
    printf("%s : d->sample_rate = [%d]\n", __func__, d->sample_rate);
    return 0;
}

static int aac_enc_set_channels(MSFilter *f, void *arg)
{
    AacEncoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (AacEncoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->channels = *((int *)arg);
    printf("%s : d->channels = [%d]\n", __func__, d->channels);
    return 0;
}


MSFilterMethod aac_enc_methods[] = {
    {MS_GET_SAMPLE_RATE, aac_enc_get_sr},
    {MS_GET_CHANNELS, aac_enc_get_channels},
    {MS_GET_SAMPLE_FMT, aac_enc_get_sf},
    {MS_GET_FRAME_SIZE,  aac_enc_get_frame_size},
    {MS_SET_SAMPLE_RATE, aac_enc_set_sr},
    {MS_SET_CHANNELS, aac_enc_set_channels},
    {-1, NULL},
};



MSFilterDesc ms_aac_enc_desc = {
    .id = MS_AAC_ENC_ID,
    .name = "AACENC",
    .text = "aac encoder",
    .category = MS_FILTER_ENCODER,
    .enc_fmt = "AAC",
    .ninputs = 1,
    .noutputs = 1,
    .init = aac_enc_init,
    .preprocess = aac_enc_preprocess,
    .process = aac_enc_process,
    .postprocess = aac_enc_postprocess,
    .uninit = aac_enc_uninit,
    .methods = aac_enc_methods,
    .flags = MS_FILTER_IS_ENABLED    
};


