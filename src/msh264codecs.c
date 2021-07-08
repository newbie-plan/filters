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
}H264Decoder;



static int decoder_init(H264Decoder *d)
{
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;

    if ((codec = avcodec_find_decoder(AV_CODEC_ID_H264)) == NULL)
    {
        fprintf(stderr, "avcodec_find_decoder failed.\n");
        return -1;
    }

    if ((codec_ctx = avcodec_alloc_context3(codec)) == NULL)
    {
        fprintf(stderr, "avcodec_alloc_context3 failed.\n");
        return -1;
    }

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

void h264_dec_init(struct _MSFilter *f)
{
    H264Decoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(H264Decoder, 1);
    memset(d, 0, sizeof(H264Decoder));
    f->data = (void *)d;
}

void h264_dec_preprocess(struct _MSFilter *f)
{
    H264Decoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    decoder_init(d);
}


void h264_dec_process(struct _MSFilter *f)
{
    H264Decoder *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Decoder *)f->data;
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
        pkt->pts = mblk_get_timestamp_info(im);
//        printf("%s : pkt->pts = [%d]\n", __func__, pkt->pts);
        if ((ret = avcodec_send_packet(d->codec_ctx, pkt)) < 0)
        {
            fprintf(stderr, "avcodec_send_packet failed : [%s].\n", av_err2str(ret));
            return;
        }

        while ((ret = avcodec_receive_frame(d->codec_ctx, frame)) >= 0)
        {
            int y,u,v;
            int frame_len = frame->height * frame->width * 3 / 2;
            om = allocb(frame_len, 0);

            for (y = 0; y < frame->height; y++)
            {
                memcpy(om->b_wptr+y*frame->width, frame->data[0]+y*frame->linesize[0], frame->width);
            }
            om->b_wptr += frame->height * frame->width;

            for (u = 0; u < frame->height / 2; u++)
            {
                memcpy(om->b_wptr+u*frame->width/2, frame->data[1]+u*frame->linesize[1], frame->width/2);
            }
            om->b_wptr += frame->height/2 * frame->width/2;

            for (v = 0; v < frame->height / 2; v++)
            {
                memcpy(om->b_wptr+v*frame->width/2, frame->data[2]+v*frame->linesize[2], frame->width/2);
            }
            om->b_wptr += frame->height/2 * frame->width/2;
//            printf("%s : frame pts = [%d]\n", __func__, frame->pts);

            mblk_set_timestamp_info(om, frame->pts);
            ms_queue_put(f->outputs[0], om);
            av_frame_unref(d->frame);
        }
        freemsg(im);
    }
}

void h264_dec_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static int decoder_uninit(H264Decoder *d)
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


void h264_dec_uninit(struct _MSFilter *f)
{
    H264Decoder *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    decoder_uninit(d);
    ms_free(d);
}


MSFilterMethod h264_dec_methods[] = {
    {-1, NULL},
};


MSFilterDesc ms_h264_dec_desc = {
    .id = MS_H264_DEC_ID,
    .name = "H264Dec",
    .text = "h264 decoder",
    .category = MS_FILTER_DECODER,
    .enc_fmt = "H264",
    .ninputs = 1,
    .noutputs = 1,
    .init = h264_dec_init,
    .preprocess = h264_dec_preprocess,
    .process = h264_dec_process,
    .postprocess = h264_dec_postprocess,
    .uninit = h264_dec_uninit,
    .methods = NULL,
    .flags = MS_FILTER_IS_ENABLED
};


typedef struct
{
    AVCodec *codec;
    AVCodecContext *codec_ctx;
    AVPacket *pkt;
    AVFrame *frame;
    int height;
    int width;
    int pix_fmt;
}H264Encoder;

static int encoder_init(H264Encoder *d)
{
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *cdc_ctx = NULL;

    if ((codec = avcodec_find_encoder(AV_CODEC_ID_H264)) == NULL)
    {
        fprintf(stderr, "avcodec_find_encoder failed.\n");
        return -1;
    }

    if ((cdc_ctx = avcodec_alloc_context3(codec)) == NULL)
    {
        fprintf(stderr, "avcodec_alloc_context3 failed.\n");
        return -1;
    }


    cdc_ctx->width = d->width;
    cdc_ctx->height = d->height;
    cdc_ctx->time_base = (AVRational){1,90000};
    cdc_ctx->framerate = (AVRational){25,1};
//    cdc_ctx->gop_size = 10;
    cdc_ctx->max_b_frames = 0;
    cdc_ctx->pix_fmt = d->pix_fmt;
    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(cdc_ctx->priv_data, "preset", "slow", 0);

    if ((ret = avcodec_open2(cdc_ctx, codec, NULL)) < 0)
    {
        fprintf(stderr, "avcodec_open2 failed.\n");
        return -1;
    }

    d->codec = codec;
    d->codec_ctx = cdc_ctx;
    d->pkt = av_packet_alloc();
    d->frame = av_frame_alloc();
    d->frame->format = cdc_ctx->pix_fmt;
    d->frame->width = cdc_ctx->width;
    d->frame->height = cdc_ctx->height;

    if ((ret = av_frame_get_buffer(d->frame, 0)) < 0)
    {
        fprintf(stderr, "av_frame_get_buffer failed.\n");
        return -1;
    }
    return 0;
}


void h264_enc_init(struct _MSFilter *f)
{
    H264Encoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(H264Encoder, 1);
    memset(d, 0, sizeof(H264Encoder));
    d->width = 1920;
    d->height = 1080;
    d->pix_fmt = AV_PIX_FMT_YUV420P;
    f->data = (void *)d;
}

void h264_enc_preprocess(struct _MSFilter *f)
{
    H264Encoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    encoder_init(d);
}


void h264_enc_process(struct _MSFilter *f)
{
    H264Encoder *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;
    int size = 0;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    size = d->width*d->height;

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        int ret = -1;

        memcpy(d->frame->data[0], im->b_rptr, size);
        im->b_rptr += size;
        memcpy(d->frame->data[1], im->b_rptr, size/4);
        im->b_rptr += size/4;
        memcpy(d->frame->data[2], im->b_rptr, size/4);
        im->b_rptr += size/4;

        d->frame->pts = mblk_get_timestamp_info(im);
//        printf("%s : frame->pts = [%d]\n", __func__, d->frame->pts);
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
//            printf("%s : pkt->pts = [%d]\n", __func__, d->pkt->pts);
            mblk_set_timestamp_info(om, d->pkt->pts);
            ms_queue_put(f->outputs[0], om);

            av_packet_unref(d->pkt);
        }
        freemsg(im);
    }
}

void h264_enc_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static int encoder_uninit(H264Encoder *d)
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

void h264_enc_uninit(struct _MSFilter *f)
{
    H264Encoder *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    encoder_uninit(d);
    ms_free(d);
}

static int h264_enc_get_width(MSFilter *f, void *arg)
{
    H264Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->width;
    return 0;
}

static int h264_enc_get_height(MSFilter *f, void *arg)
{
    H264Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->height;
    return 0;
}

static int h264_enc_get_pf(MSFilter *f, void *arg)
{
    H264Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->pix_fmt;
    return 0;
}

static int h264_enc_set_width(MSFilter *f, void *arg)
{
    H264Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->width = *((int *)arg);
    printf("%s : width = [%d]\n", __func__, d->width);
    return 0;
}

static int h264_enc_set_height(MSFilter *f, void *arg)
{
    H264Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->height = *((int *)arg);
    printf("%s : height = [%d]\n", __func__, d->height);
    return 0;
}

static int h264_enc_set_pf(MSFilter *f, void *arg)
{
    H264Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->pix_fmt = *((int *)arg);
    return 0;
}


MSFilterMethod h264_enc_methods[] = {
    {MS_GET_WIDTH, h264_enc_get_width},
    {MS_GET_HEIGTH, h264_enc_get_height},
    {MS_GET_PIX_FMT, h264_enc_get_pf},
    {MS_SET_WIDTH, h264_enc_set_width},
    {MS_SET_HEIGTH, h264_enc_set_height},
    {MS_SET_PIX_FMT, h264_enc_set_pf},
    {-1, NULL},
};

MSFilterDesc ms_h264_enc_desc = {
    .id = MS_H264_ENC_ID,
    .name = "H264ENC",
    .text = "h264 encoder",
    .category = MS_FILTER_ENCODER,
    .enc_fmt = "H264",
    .ninputs = 1,
    .noutputs = 1,
    .init = h264_enc_init,
    .preprocess = h264_enc_preprocess,
    .process = h264_enc_process,
    .postprocess = h264_enc_postprocess,
    .uninit = h264_enc_uninit,
    .methods = h264_enc_methods,
    .flags = MS_FILTER_IS_ENABLED
};


