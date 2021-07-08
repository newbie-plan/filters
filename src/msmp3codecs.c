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
    int sample_fmt;
    MSQueue split_before;
    MSQueue split_after;
    FILE *fp;
}MP3Decoder;

#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define MPA_STEREO  0
#define MPA_JSTEREO 1
#define MPA_DUAL    2
#define MPA_MONO    3

typedef struct MPADecodeHeader
{
    int frame_size;
    int error_protection;
    int layer;
    int sample_rate;
    int sample_rate_index; /* between 0 and 8 */
    int bit_rate;
    int nb_channels;
    int mode;
    int mode_ext;
    int lsf;
}MPADecodeHeader;

const uint16_t avpriv_mpa_bitrate_tab[2][3][15] = {
    { {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
      {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384 },
      {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320 } },
    { {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256},
      {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160},
      {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}
    }
};

const uint16_t avpriv_mpa_freq_tab[3] = { 44100, 48000, 32000 };


static int decoder_init(MP3Decoder *d)
{
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;

    if ((codec = avcodec_find_decoder(AV_CODEC_ID_MP3)) == NULL)
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

void mp3_dec_init(struct _MSFilter *f)
{
    MP3Decoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(MP3Decoder, 1);
    memset(d, 0, sizeof(MP3Decoder));
    d->sample_rate = 44100;
    d->channels = 1;
    d->sample_fmt = AV_SAMPLE_FMT_FLTP;
    ms_queue_init(&d->split_before);
    ms_queue_init(&d->split_after);
    d->fp = fopen("audio.mp3", "wb");
    f->data = (void *)d;
}

void mp3_dec_preprocess(struct _MSFilter *f)
{
    MP3Decoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MP3Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    decoder_init(d);
}


/* fast header check for resync */
static inline int ff_mpa_check_header(uint32_t header){
    /* header */
    if ((header & 0xffe00000) != 0xffe00000)
        return -1;
    /* version check */
    if ((header & (3<<19)) == 1<<19)
        return -1;
    /* layer check */
    if ((header & (3<<17)) == 0)
        return -1;
    /* bit rate */
    if ((header & (0xf<<12)) == 0xf<<12)
        return -1;
    /* frequency */
    if ((header & (3<<10)) == 3<<10)
        return -1;
    return 0;
}

static int avpriv_mpegaudio_decode_header(MPADecodeHeader *s, uint32_t header)
{
    int sample_rate, frame_size, mpeg25, padding;
    int sample_rate_index, bitrate_index;
    int ret;
    unsigned char *abc = (unsigned char *)&header;

    ret = ff_mpa_check_header(header);
    if (ret < 0)
        return ret;

    if (header & (1<<20)) {
        s->lsf = (header & (1<<19)) ? 0 : 1;
        mpeg25 = 0;
    } else {
        s->lsf = 1;
        mpeg25 = 1;
    }

    s->layer = 4 - ((header >> 17) & 3);
    /* extract frequency */
    sample_rate_index = (header >> 10) & 3;
    if (sample_rate_index >= FF_ARRAY_ELEMS(avpriv_mpa_freq_tab))
        sample_rate_index = 0;
    sample_rate = avpriv_mpa_freq_tab[sample_rate_index] >> (s->lsf + mpeg25);
    sample_rate_index += 3 * (s->lsf + mpeg25);
    s->sample_rate_index = sample_rate_index;
    s->error_protection = ((header >> 16) & 1) ^ 1;
    s->sample_rate = sample_rate;

    bitrate_index = (header >> 12) & 0xf;
    padding = (header >> 9) & 1;
    //extension = (header >> 8) & 1;
    s->mode = (header >> 6) & 3;
    s->mode_ext = (header >> 4) & 3;
    //copyright = (header >> 3) & 1;
    //original = (header >> 2) & 1;
    //emphasis = header & 3;

    if (s->mode == MPA_MONO)
        s->nb_channels = 1;
    else
        s->nb_channels = 2;

    if (bitrate_index != 0) {
        frame_size = avpriv_mpa_bitrate_tab[s->lsf][s->layer - 1][bitrate_index];
        s->bit_rate = frame_size * 1000;
        switch(s->layer) {
        case 1:
            frame_size = (frame_size * 12000) / sample_rate;
            frame_size = (frame_size + padding) * 4;
            break;
        case 2:
            frame_size = (frame_size * 144000) / sample_rate;
            frame_size += padding;
            break;
        default:
        case 3:
            frame_size = (frame_size * 144000) / (sample_rate << s->lsf);
            frame_size += padding;
            break;
        }
        s->frame_size = frame_size;
    } else {
        /* if no frame size computed, signal it */
        return 1;
    }
    return 0;
}

static void mp3_split_frame(MP3Decoder *d)
{
    MPADecodeHeader head;
    mblk_t *im = NULL;
    mblk_t *om = NULL;

    while ((im = ms_queue_get(&d->split_before)) != NULL)
    {
        int len = im->b_wptr - im->b_rptr;
        memset(&head, 0, sizeof(MPADecodeHeader));
        while (len)
        {
            avpriv_mpegaudio_decode_header(&head, ntohl(*(uint32_t *)im->b_rptr));
            if (head.frame_size > 0)
            {
                om = allocb(head.frame_size, 0);
                memcpy(om->b_wptr, im->b_rptr, head.frame_size);
                om->b_wptr += head.frame_size;
                ms_queue_put(&d->split_after, om);
                len -= head.frame_size;
                im->b_rptr += head.frame_size;
            }
        }
        freemsg(im);
    }
}

void mp3_dec_process(struct _MSFilter *f)
{
    MP3Decoder *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MP3Decoder *)f->data;
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
        int bytes_per_sample = av_get_bytes_per_sample(d->codec_ctx->sample_fmt);

        if (d->fp != NULL)
        {
            fwrite(im->b_rptr, 1, im->b_wptr - im->b_rptr, d->fp);
        }

        ms_queue_put(&d->split_before, im);
        mp3_split_frame(d);
        while ((im = ms_queue_get(&d->split_after)) != NULL)
        {
            pkt->data = im->b_rptr;
            pkt->size = im->b_wptr - im->b_rptr;

            if ((ret = avcodec_send_packet(d->codec_ctx, pkt)) < 0)
            {
                fprintf(stderr, "avcodec_send_packet failed : [%s].\n", av_err2str(ret));
                return;
            }

            while ((ret = avcodec_receive_frame(d->codec_ctx, frame)) >= 0)
            {
                int frame_len = frame->nb_samples * bytes_per_sample;
                om = allocb(frame_len, 0);
                memcpy(om->b_wptr, frame->data[0], frame_len);
                om->b_wptr += frame_len;
                ms_queue_put(f->outputs[0], om);
                av_frame_unref(d->frame);
            }
            freemsg(im);
        }
    }

}

void mp3_dec_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static int decoder_uninit(MP3Decoder *d)
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


void mp3_dec_uninit(struct _MSFilter *f)
{
    MP3Decoder *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MP3Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    decoder_uninit(d);
    ms_queue_flush(&d->split_before);
    ms_queue_flush(&d->split_after);
    ms_free(d);
}


static int mp3_dec_get_sr(MSFilter *f, void *arg)
{
    MP3Decoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MP3Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_rate;
    return 0;
}

static int mp3_dec_get_channels(MSFilter *f, void *arg)
{
    MP3Decoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MP3Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->channels;
    return 0;
}

static int mp3_dec_get_sf(MSFilter *f, void *arg)
{
    MP3Decoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MP3Decoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_fmt;
    return 0;
}


MSFilterMethod mp3_dec_methods[] = {
    {MS_GET_SAMPLE_RATE, mp3_dec_get_sr},
    {MS_GET_CHANNELS,    mp3_dec_get_channels},
    {MS_GET_SAMPLE_FMT,  mp3_dec_get_sf},
    {-1, NULL},
};


MSFilterDesc ms_mp3_dec_desc = {
    .id = MS_MP3_DEC_ID,
    .name = "MP3Dec",
    .text = "mp3 decoder",
    .category = MS_FILTER_DECODER,
    .enc_fmt = "MP3",
    .ninputs = 1,
    .noutputs = 1,
    .init = mp3_dec_init,
    .preprocess = mp3_dec_preprocess,
    .process = mp3_dec_process,
    .postprocess = mp3_dec_postprocess,
    .uninit = mp3_dec_uninit,
    .methods = NULL,
    .flags = MS_FILTER_IS_ENABLED
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
}Mp3Encoder;

static int encoder_init(Mp3Encoder *d)
{
    int ret = -1;
    AVCodec *codec = NULL;
    AVCodecContext *cdc_ctx = NULL;

    if ((codec = avcodec_find_encoder(AV_CODEC_ID_MP3)) == NULL)
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


void mp3_enc_init(struct _MSFilter *f)
{
    Mp3Encoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(Mp3Encoder, 1);
    d->sample_rate = 44100;
    d->channels = 1;
    d->sample_fmt = AV_SAMPLE_FMT_S16P;
    d->frame_size = 1152;
    d->bit_rate = 192000;

    ms_bufferizer_init(&d->encoder);
    f->data = (void *)d;
}

void mp3_enc_preprocess(struct _MSFilter *f)
{
    Mp3Encoder *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    encoder_init(d);
}


void mp3_enc_process(struct _MSFilter *f)
{
    Mp3Encoder *d = NULL;
    mblk_t *im = NULL;
    mblk_t *om = NULL;
    int data_size = 0;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
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

void mp3_enc_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static int encoder_uninit(Mp3Encoder *d)
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

void mp3_enc_uninit(struct _MSFilter *f)
{
    Mp3Encoder *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    ms_bufferizer_flush(&d->encoder);
    encoder_uninit(d);
    ms_free(d);
}

static int mp3_enc_get_sr(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_rate;
    return 0;
}

static int mp3_enc_get_channels(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->channels;
    return 0;
}

static int mp3_enc_get_sf(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->sample_fmt;
    return 0;
}

static int mp3_enc_get_frame_size(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    *((int *)arg) = d->frame_size;
    return 0;
}

static int mp3_enc_set_sr(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->sample_rate = *((int *)arg);
    printf("%s : d->sample_rate = [%d]\n", __func__, d->sample_rate);
    return 0;
}

static int mp3_enc_set_channels(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d->channels = *((int *)arg);
    printf("%s : d->channels = [%d]\n", __func__, d->channels);
    return 0;
}

static int mp3_enc_set_sf(MSFilter *f, void *arg)
{
    Mp3Encoder *d = NULL;

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (Mp3Encoder *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->sample_fmt = *((int *)arg);
    return 0;
}

MSFilterMethod mp3_enc_methods[] = {
    {MS_GET_SAMPLE_RATE, mp3_enc_get_sr},
    {MS_GET_CHANNELS,    mp3_enc_get_channels},
    {MS_GET_SAMPLE_FMT,  mp3_enc_get_sf},
    {MS_GET_FRAME_SIZE,  mp3_enc_get_frame_size},
    {MS_SET_SAMPLE_RATE, mp3_enc_set_sr},
    {MS_SET_CHANNELS,    mp3_enc_set_channels},
    {MS_SET_SAMPLE_FMT,  mp3_enc_set_sf},
    {-1, NULL},
};

MSFilterDesc ms_mp3_enc_desc = {
    .id = MS_MP3_ENC_ID,
    .name = "MP3ENC",
    .text = "mp3 encoder",
    .category = MS_FILTER_ENCODER,
    .enc_fmt = "MP3",
    .ninputs = 1,
    .noutputs = 1,
    .init = mp3_enc_init,
    .preprocess = mp3_enc_preprocess,
    .process = mp3_enc_process,
    .postprocess = mp3_enc_postprocess,
    .uninit = mp3_enc_uninit,
    .methods = mp3_enc_methods,
    .flags = MS_FILTER_IS_ENABLED
};


