#include <stdio.h>
#include <string.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>


typedef struct
{
    AVCodec *codec;
    AVCodecContext *codec_ctx;
    AVPacket *pkt;
    AVFrame *frame;
    int sample_rate;
    int channels;
}G711Decoder;



static int decoder_init(G711Decoder *d)
{
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;

    if ((codec = avcodec_find_decoder(AV_CODEC_ID_PCM_MULAW)) == NULL)
    {
        fprintf(stderr, "avcodec_find_decoder failed.\n");
        return -1;
    }

    if ((codec_ctx = avcodec_alloc_context3(codec)) == NULL)
    {
        fprintf(stderr, "avcodec_alloc_context3 failed.\n");
        return -1;
    }

    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16P;
    codec_ctx->sample_rate = d->sample_rate;
    codec_ctx->channel_layout = av_get_default_channel_layout(d->channels);
    codec_ctx->channels = d->channels;

    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0)
    {
        fprintf(stderr, "avcodec_open2 failed.\n");
        return -1;
    }

    d->pkt = av_packet_alloc();
    d->frame = av_frame_alloc();
    d->codec = codec;
    d->codec_ctx = codec_ctx;
    return 0;
}


void g711_dec_init(struct _MSFilter *f)
{
    G711Decoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(G711Decoder, 1);
    memset(d, 0, sizeof(G711Decoder));
    d->sample_rate = 8000;
    d->channels = 1;
    decoder_init(d);
    f->data = (void *)d;
}

void g711_dec_preprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}


void g711_dec_process(struct _MSFilter *f)
{
    G711Decoder *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (G711Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        int ret = -1;
        AVPacket *pkt = d->pkt;
        AVFrame *frame = d->frame;

        pkt->data = im->b_rptr;
        pkt->size = im->b_wptr - im->b_rptr;

        if ((ret = avcodec_send_packet(d->codec_ctx, pkt)) < 0)
        {
            fprintf(stderr, "avcodec_send_packet failed : [%s].\n", av_err2str(ret));
            return;
        }
        
        while ((ret = avcodec_receive_frame(d->codec_ctx, frame)) >= 0)
        {
            om = allocb(frame->nb_samples*2, 0);
            memcpy(om->b_wptr, frame->data[0], frame->nb_samples*2);
            om->b_wptr += frame->nb_samples*2;
            ms_queue_put(f->outputs[0], om);
            av_frame_unref(d->frame);
        }

        freemsg(im);
    }
}

void g711_dec_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}


static int decoder_uninit(G711Decoder *d)
{
    if (d->codec_ctx != NULL)
    {
        avcodec_free_context(&d->codec_ctx);
    }
    if (d->pkt != NULL)
    {
        av_packet_free(&d->pkt);
    }
    if (d->frame != NULL)
    {
        av_frame_free(&d->frame);
    }

    return 0;
}


void g711_dec_uninit(struct _MSFilter *f)
{
    G711Decoder *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (G711Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    decoder_uninit(d);
    ms_free(d);
}


static int g711_dec_get_sr(MSFilter *f, void *arg)
{
    G711Decoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (G711Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_rate;
    return 0;
}

static int g711_dec_get_channels(MSFilter *f, void *arg)
{
    G711Decoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (G711Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->channels;
    return 0;
}

static int g711_dec_get_sf(MSFilter *f, void *arg)
{
    G711Decoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (G711Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->codec_ctx->sample_fmt;
    return 0;
}


MSFilterMethod g711_dec_methods[] = {
    {MS_GET_SAMPLE_RATE,    g711_dec_get_sr},
    {MS_GET_CHANNELS,       g711_dec_get_channels},
    {MS_GET_SAMPLE_FMT,     g711_dec_get_sf},
    {-1, NULL},
};


MSFilterDesc ms_g711_dec_desc = {
    .id = MS_G711_DEC_ID,
    .name = "G711Dec",
    .text = "g711 decoder",
    .category = MS_FILTER_DECODER,
    .enc_fmt = "G711",
    .ninputs = 1,
    .noutputs = 1,
    .init = g711_dec_init,
    .preprocess = g711_dec_preprocess,
    .process = g711_dec_process,
    .postprocess = g711_dec_postprocess,
    .uninit = g711_dec_uninit,
    .methods = g711_dec_methods,
    .flags = MS_FILTER_IS_ENABLED
};







