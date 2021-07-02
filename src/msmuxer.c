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
    const char *file_name;
    AVFormatContext *fmt_ctx;
    AVStream *audio_stream;
    AVStream *video_stream;
    int audio_stream_index;
    int video_stream_index;
    AVPacket *pkt;
    int channels;
    int sample_rate;
    int sample_fmt;
    int frame_size;
    int codec_id;
    int heigth;
    int width;
    int64_t last_pts_video; 
    int packet_num_a;
}MuxerData;

static int create_new_stream(MuxerData *d)
{
    int ret = -1;
    AVFormatContext *fmt_ctx = NULL;
    AVStream *audio_stream = NULL;
    AVStream *video_stream = NULL;

    if ((ret = avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, d->file_name)) < 0)
    {
        fprintf(stderr, "avformat_alloc_outpot_context2 failed.\n");
        return -1;
    }

    /*video stream*/
    if ((video_stream = avformat_new_stream(fmt_ctx, NULL)) == NULL)
    {
        fprintf(stderr, "avformat_new_stream for failed.\n");
        return -1;
    }
    video_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    video_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    video_stream->codecpar->codec_tag = 0;
    video_stream->codecpar->format = AV_PIX_FMT_YUV420P;
    video_stream->codecpar->width = d->width;
    video_stream->codecpar->height = d->heigth;

    /*audio stream*/
    if ((audio_stream = avformat_new_stream(fmt_ctx, NULL)) == NULL)
    {
        fprintf(stderr, "avformat_new_stream for failed.\n");
        return -1;
    }
    audio_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    audio_stream->codecpar->codec_id = d->codec_id;
    audio_stream->codecpar->codec_tag = 0;
    audio_stream->codecpar->format = d->sample_fmt;
    audio_stream->codecpar->channel_layout = av_get_default_channel_layout(d->channels);
    audio_stream->codecpar->channels = d->channels;
    audio_stream->codecpar->sample_rate = d->sample_rate;
    audio_stream->codecpar->frame_size = d->frame_size;

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&fmt_ctx->pb, d->file_name, AVIO_FLAG_WRITE) < 0)
        {
            fprintf(stderr, "avio_open failed.\n");
            return -1;
        }
    }

    av_dump_format(fmt_ctx, 0, d->file_name, 1);

    if ((ret = avformat_write_header(fmt_ctx, NULL)) < 0)
    {
        fprintf(stderr, "avformat_write_header failed.\n");
        return -1;
    }

    d->fmt_ctx = fmt_ctx;
    d->audio_stream = audio_stream;
    d->video_stream = video_stream;
    d->audio_stream_index = audio_stream->index;
    d->video_stream_index = video_stream->index;
    return 0;
}


void muxer_dec_init(struct _MSFilter *f)
{
    MuxerData *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(MuxerData, 1);
    memset(d, 0, sizeof(MuxerData));
    d->channels = 1;
    d->sample_rate = 44100;
    d->sample_fmt = AV_SAMPLE_FMT_S16P;
    d->frame_size = 1152;
    d->heigth = 1920;
    d->width = 1080;
    d->pkt = av_packet_alloc();
    f->data = (void *)d;
}

void muxer_dec_preprocess(struct _MSFilter *f)
{
    MuxerData *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    create_new_stream(d);
}


void muxer_dec_process(struct _MSFilter *f)
{
    MuxerData *d = NULL;
    mblk_t *im = NULL;

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        d->pkt->data = im->b_rptr;
        d->pkt->size = im->b_wptr - im->b_rptr;

        d->pkt->pts = d->packet_num_a++ * d->frame_size;
        d->pkt->duration = d->frame_size;
        d->pkt->dts = d->pkt->pts;
        d->pkt->pos = -1;
        d->pkt->stream_index = d->audio_stream_index;

        if (av_interleaved_write_frame(d->fmt_ctx, d->pkt) < 0)
        {
            fprintf(stderr, "av_interleaved_write_frame failed.\n");
            return;
        }

        freemsg(im);
    }

    while ((im = ms_queue_get(f->inputs[1])) != NULL)
    {
        d->pkt->data = im->b_rptr;
        d->pkt->size = im->b_wptr - im->b_rptr;

        d->pkt->pts = mblk_get_timestamp_info(im);
        d->pkt->duration = 3750;
        d->pkt->dts = d->pkt->pts;
        d->pkt->pos = -1;
        d->pkt->stream_index = d->video_stream_index;

        if (av_interleaved_write_frame(d->fmt_ctx, d->pkt) < 0)
        {
            fprintf(stderr, "av_interleaved_write_frame failed.\n");
            return;
        }

        d->last_pts_video = d->pkt->pts;
        freemsg(im);
    }


}

void muxer_dec_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

void muxer_dec_uninit(struct _MSFilter *f)
{
    MuxerData *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    av_write_trailer(d->fmt_ctx);

    if (d->pkt != NULL)
    {
        av_packet_free(&d->pkt);
    }

    if (!(d->fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(d->fmt_ctx->pb);
    }
    if (d->fmt_ctx != NULL)
    {
        avformat_free_context(d->fmt_ctx);
    }

    ms_free(d);
}




static int muxer_set_file(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->file_name = (const char *)arg;
    printf("%s : file name = [%s]\n", __func__, d->file_name);
    return 0;
}

static int muxer_set_sr(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->sample_rate = *((int *)arg);
    printf("%s : sample rate = [%d]\n", __func__, d->sample_rate);
    return 0;
}

static int muxer_set_channels(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;
    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->channels = *((int *)arg);
    printf("%s : channels = [%d]\n", __func__, d->channels);
    return 0;
}

static int muxer_set_sf(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->sample_fmt = *((int *)arg);
    return 0;
}

static int muxer_set_frame_size(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;
    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->frame_size = *((int *)arg);
    return 0;
}

static int muxer_set_mime_type(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;
    char *mime_type = (char *)arg;
    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    if (strcasecmp(mime_type, "mp3") == 0)
    {
        d->codec_id = AV_CODEC_ID_MP3;
        d->sample_fmt = AV_SAMPLE_FMT_FLTP;
        d->frame_size = d->sample_rate <= 16000 ? 576 : 1152;
    }
    else if (strcasecmp(mime_type, "aac") == 0)
    {
        d->codec_id = AV_CODEC_ID_AAC;
        d->sample_fmt = AV_SAMPLE_FMT_FLTP;
        d->frame_size = 1024;
    }
    else
    {
        d->codec_id = AV_CODEC_ID_MP3;
        d->sample_fmt = AV_SAMPLE_FMT_FLTP;
        d->frame_size = d->sample_rate <= 16000 ? 576 : 1152;
    }
    printf("%s : d->mime_type = [%s]\n", __func__, mime_type);
    return 0;
}


static int muxer_set_width(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->width = *((int *)arg);
    printf("%s : width = [%d]\n", __func__, d->width);
    return 0;
}

static int muxer_set_heigth(MSFilter *f, void *arg)
{
    MuxerData *d = NULL;

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (MuxerData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->heigth = *((int *)arg);
    printf("%s : heigth = [%d]\n", __func__, d->heigth);
    return 0;
}

MSFilterMethod muxer_methods[] = {
    {MS_SET_FILE_NAME, muxer_set_file},
    {MS_SET_SAMPLE_RATE, muxer_set_sr},
    {MS_SET_CHANNELS, muxer_set_channels},
    {MS_SET_SAMPLE_FMT, muxer_set_sf},
    {MS_SET_FRAME_SIZE, muxer_set_frame_size},
    {MS_SET_MIME_TYPE, muxer_set_mime_type},
    {MS_SET_WIDTH, muxer_set_width},
    {MS_SET_HEIGTH, muxer_set_heigth},
    {-1, NULL},
};



MSFilterDesc ms_muxer_desc = {
    .id = MS_MUXER_ID,
    .name = "Muxer",
    .text = "audio and video muxer",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 2,
    .noutputs = 0,
    .init = muxer_dec_init,
    .preprocess = muxer_dec_preprocess,
    .process = muxer_dec_process,
    .postprocess = muxer_dec_postprocess,
    .uninit = muxer_dec_uninit,
    .methods = muxer_methods,
};


