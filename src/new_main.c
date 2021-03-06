#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <base/msfactory.h>
#include <base/msfilter.h>
#include <base/msticker.h>
#include <base/mscommon.h>
#include <libavutil/samplefmt.h>
#include <libavutil/pixfmt.h>

#define DEFAULT_MIME_TYPE "aac"
#define MAX_STREAM_NUM 2

typedef struct Parameter
{
    int     input_file_count;
    int     input_stream_count;
    struct Input
    {
        char *  input_file;
        char *  input_src_addr;
        char *  input_dst_addr;
        int     input_sample_rate;
        int     input_channels;
        int     input_sample_fmt;
        char *  input_mime_type;
        int     input_width;
        int     input_height;
        int     input_pix_fmt;
    }in[MAX_STREAM_NUM];
    char *  output_file;
    int     output_sample_rate;
    int     output_channels;
    int     output_sample_fmt;
    char *  output_mime_type;
    int     output_width;
    int     output_height;
    int     output_pix_fmt;
}Parameter;

typedef struct PcapStream
{
    MSTicker *ticker;
    MSFilter *source[MAX_STREAM_NUM];
    struct Audio
    {
        MSFilter *decoder[MAX_STREAM_NUM];
        MSFilter *resample;
        MSFilter *encoder;
        MSFilter *amix;
    }audio;
    struct Video
    {
        MSFilter *regroup[MAX_STREAM_NUM];
        MSFilter *decoder[MAX_STREAM_NUM];
        MSFilter *scale;
        MSFilter *encoder;
        MSFilter *vmix;
    }video;
    MSFilter *muxer;
    int sample_rate;
    int channels;
    int sample_fmt;
    int width;
    int height;
    int pix_fmt;
}PcapStream;


static struct option long_options[] = {
    {"in",          required_argument,  NULL, 'i' },
    {"out",         required_argument,  NULL, 'o' },
    {"srcaddr",     required_argument,  NULL, '1' },
    {"dstaddr",     required_argument,  NULL, 'd' },
    {"sample_rate", required_argument,  NULL, 'r' },
    {"channels",    required_argument,  NULL, 'c' },
    {"format",      required_argument,  NULL, 'f' },
    {"acodec",      required_argument,  NULL, 'a' },
    {"size",        required_argument,  NULL, 's' },
    {"pix_fmt",     required_argument,  NULL, 'p' },
    {"help",        no_argument,        NULL, 'h' },
    {0,             0,                  0,     0  }
};

static void print_usage()
{
    printf("Option:\n");
    printf("The option marked with * is required.\n");
    printf(" -i, --in=FILE       *       The file name for input file.\n");
    printf(" -o, --out=FILE      *       The file name for output file.\n");
    printf(" --srcaddr=IP        *       The address to be filtered, src-dst.\n");
    printf(" --dstaddr=IP        *       The address to be filtered, src-dst.\n");
    printf(" -r, --sample_rate=          The sample rate of audio.\n");
    printf(" -c, --channels=             The channels rate of audio.\n");
    printf(" -f, --format,       *       The format of audio, eg: s16p/fltp.\n");
    printf(" -a, --acodec,       *       The codec of audio.\n");
    printf(" -s, --size,                 The resolution of video.\n");
    printf(" -p, --pix_fmt,              The format of video, eg: yuv420P/yuv420.\n");
    printf(" -h, --help                  Print this message and exit.\n");
}

static void parse_options(int argc, const char *argv[], Parameter *param)
{
    int optc = -1;
    int opt_index = -1;
    int sample_rate = 0;
    int channels = 0;
    int sample_fmt = AV_SAMPLE_FMT_FLTP;
    char *mime_type = NULL;
    char *src_ip_addr = NULL;
    char *dst_ip_addr = NULL;
    int width = 0;
    int height = 0;
    int pix_fmt = AV_PIX_FMT_YUV420P;

    while ((optc = getopt_long(argc, (char *const *)argv, "hi::o:r:c:f:a:s:p:", long_options, &opt_index)) != -1)
    {
        printf("optc = [%c] : optarg = [%s]\n", optc, optarg);
        switch (optc)
        {
            case 'i':
            {
                param->in[param->input_stream_count].input_file = optarg;
                param->in[param->input_stream_count].input_src_addr = src_ip_addr;
                param->in[param->input_stream_count].input_dst_addr = dst_ip_addr;
                param->in[param->input_stream_count].input_sample_rate = sample_rate;
                param->in[param->input_stream_count].input_channels = channels;
                param->in[param->input_stream_count].input_sample_fmt = sample_fmt;
                param->in[param->input_stream_count].input_mime_type = mime_type;
                param->in[param->input_stream_count].input_width = width;
                param->in[param->input_stream_count].input_height = height;
                param->in[param->input_stream_count].input_pix_fmt = pix_fmt;
                param->input_stream_count++;
                if (optarg != NULL) param->input_file_count++;

                sample_rate = 0;
                channels = 0;
                sample_fmt = AV_SAMPLE_FMT_FLTP;
                mime_type = NULL;
                src_ip_addr = NULL;
                dst_ip_addr = NULL;
                width = 0;
                height = 0;
                pix_fmt = AV_PIX_FMT_YUV420P;
                break;
            }
            case 'o':
            {
                param->output_file = optarg;
                param->output_sample_rate = sample_rate;
                param->output_channels = channels;
                param->output_sample_fmt = sample_fmt;
                param->output_mime_type = mime_type;
                param->output_width = width;
                param->output_height = height;
                param->output_pix_fmt = pix_fmt;

                sample_rate = 0;
                channels = 0;
                sample_fmt = AV_SAMPLE_FMT_FLTP;
                mime_type = NULL;
                width = 0;
                height = 0;
                pix_fmt = AV_PIX_FMT_YUV420P;
                break;
            }
            case '1':
            {
                src_ip_addr = optarg;
                break;
            }
            case 'd':
            {
                dst_ip_addr = optarg;
                break;
            }
            case 'r':
            {
                sample_rate = atoi(optarg);
                break;
            }
            case 'c':
            {
                channels = atoi(optarg);
                break;
            }
            case 'f':
            {
                sample_fmt = av_get_sample_fmt(optarg);
                break;
            }
            case 'a':
            {
                mime_type = optarg;
                break;
            }
            case 's':
            {
                sscanf(optarg, "%dx%d", &width, &height);
                break;
            }
            case 'p':
            {
                pix_fmt = av_get_pix_fmt(optarg);
                break;
            }
            case '?':
            case 'h':
            default:
            {
                print_usage();
                exit(0);
            }
        }
    }
}


void ms_filter_preprocess(MSFilter *f, struct _MSTicker *t)
{
    f->last_tick = 0;
    f->ticker = t;
    if (f->desc->preprocess != NULL) f->desc->preprocess(f);
}

static int ticker_attach(MSTicker *ticker, PcapStream *stream)
{
    bctbx_list_t *it = NULL;
    bctbx_list_t *filters = NULL;

    filters = bctbx_list_append(filters, stream->source[0]);
    filters = bctbx_list_append(filters, stream->source[1]);
    if (stream->audio.decoder[0])  filters = bctbx_list_append(filters, stream->audio.decoder[0]);
    if (stream->audio.decoder[1])  filters = bctbx_list_append(filters, stream->audio.decoder[1]);
    filters = bctbx_list_append(filters, stream->video.regroup[0]);
    filters = bctbx_list_append(filters, stream->video.regroup[1]);
    if (stream->video.decoder[0])  filters = bctbx_list_append(filters, stream->video.decoder[0]);
    if (stream->video.decoder[1])  filters = bctbx_list_append(filters, stream->video.decoder[1]);
    filters = bctbx_list_append(filters, stream->audio.amix);
    filters = bctbx_list_append(filters, stream->video.vmix);
    if (stream->audio.encoder)  filters = bctbx_list_append(filters, stream->audio.encoder);
    if (stream->video.encoder)  filters = bctbx_list_append(filters, stream->video.encoder);
    filters = bctbx_list_append(filters, stream->muxer);

    for(it = filters; it != NULL; it = it->next)
    {
        ms_filter_preprocess((MSFilter*)it->data, ticker);
    }    

    ticker->execution_list = bctbx_list_concat(ticker->execution_list, filters);
}

static void pcap_file_end(void *userdata, struct _MSFilter *f, unsigned int id, void *arg)
{
    printf("%s : id =  [%d]\n", __func__, id);
    if (id == 0)
    {
        ms_ticker_destroy(f->ticker);
    }
}


/*
 * note: since not all filters implement MS_FILTER_GET_SAMPLE_RATE and MS_FILTER_GET_NCHANNELS, the PayloadType passed here is used to guess this information.
 */
static void stream_configure_resampler(PcapStream *st, MSFilter *resampler, MSFilter *from, MSFilter *to)
{
    int from_rate = 0, to_rate = 0;
    int from_channels = 0, to_channels = 0;
    int from_sample_fmt = 0, to_sample_fmt = 0;
    ms_filter_call_method(from, MS_GET_SAMPLE_RATE, &from_rate);
    ms_filter_call_method(to, MS_GET_SAMPLE_RATE, &to_rate);
    ms_filter_call_method(from, MS_GET_CHANNELS, &from_channels);
    ms_filter_call_method(to, MS_GET_CHANNELS, &to_channels);
    ms_filter_call_method(from, MS_GET_SAMPLE_FMT, &from_sample_fmt);
    ms_filter_call_method(to, MS_GET_SAMPLE_FMT, &to_sample_fmt);

    // Access name member only if filter desc member is not null to aviod segfaults
    const char * from_name = (from) ? ((from->desc) ? from->desc->name : "Unknown") : "Unknown";
    const char * to_name = (to) ? ((to->desc) ? to->desc->name : "Unknown" ) : "Unknown";

    if (from_channels == 0)
    {
        from_channels = st->channels;
        printf("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method\n", from_name);
    }
    if (to_channels == 0)
    {
        to_channels = st->channels;
        printf("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method\n", to_name);
    }
    if (from_rate == 0)
    {
        printf("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method\n", from_name);
        from_rate = st->sample_rate;
    }
    if (to_rate == 0)
    {
        printf("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method", to_name);
        to_rate = st->sample_rate;
    }
    if (from_sample_fmt == 0)
    {
        printf("Filter %s does not implement the MS_GET_SAMPLE_FMT method\n", from_name);
        from_sample_fmt = st->sample_fmt;
    }
    if (to_sample_fmt == 0)
    {
        printf("Filter %s does not implement the MS_GET_SAMPLE_FMT method", to_name);
        to_sample_fmt = st->sample_fmt;
    }
    ms_filter_call_method(resampler, MS_SET_SAMPLE_RATE, &from_rate);
    ms_filter_call_method(resampler, MS_SET_OUTPUT_SAMPLE_RATE, &to_rate);
    ms_filter_call_method(resampler, MS_SET_CHANNELS, &from_channels);
    ms_filter_call_method(resampler, MS_SET_OUTPUT_CHANNELS, &to_channels);
    ms_filter_call_method(resampler, MS_SET_SAMPLE_FMT, &from_sample_fmt);
    ms_filter_call_method(resampler, MS_SET_OUTPUT_SAMPLE_FMT, &to_sample_fmt);
    printf("configuring %s : %p --> %s : %p from rate [%i] to rate [%i] and from channel [%i] to channel [%i]\n",
        from_name, from, to_name, to, from_rate, to_rate, from_channels, to_channels);
}

static void stream_configure_scale(PcapStream *st, MSFilter *scale, MSFilter *from, MSFilter *to)
{
    int from_width = 0, to_width = 0;
    int from_height = 0, to_height = 0;

    ms_filter_call_method(from, MS_GET_WIDTH, &from_width);
    ms_filter_call_method(to, MS_GET_WIDTH, &to_width);
    ms_filter_call_method(from, MS_GET_HEIGTH, &from_height);
    ms_filter_call_method(to, MS_GET_HEIGTH, &to_height);

    // Access name member only if filter desc member is not null to aviod segfaults
    const char * from_name = (from) ? ((from->desc) ? from->desc->name : "Unknown") : "Unknown";
    const char * to_name = (to) ? ((to->desc) ? to->desc->name : "Unknown" ) : "Unknown";

    if (from_width == 0)
    {
        from_width = st->width;
        printf("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method\n", from_name);
    }
    if (to_width == 0)
    {
        to_width = st->width;
        printf("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method\n", to_name);
    }
    if (from_height == 0)
    {
        from_height = st->height;
        printf("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method\n", from_name);
    }
    if (to_height == 0)
    {
        to_height = st->height;
        printf("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method", to_name);
    }

    ms_filter_call_method(scale, MS_SET_WIDTH, &from_width);
    ms_filter_call_method(scale, MS_SET_HEIGTH, &from_height);
    ms_filter_call_method(scale, MS_SET_OUTPUT_WIDTH, &to_width);
    ms_filter_call_method(scale, MS_SET_OUTPUT_HEIGTH, &to_height);
    printf("configuring %s : %p --> %s : %p from width [%i] to width [%i] and from height [%i] to height [%i]\n",
        from_name, from, to_name, to, from_width, to_width, from_height, to_height);
}



static int pcap_stream_start_from_param(MSFactory *factory, PcapStream *stream, Parameter *param)
{
    MSConnectionHelper h;
    int i, ret = -1;
    int src_sample_rate = param->in[0].input_sample_rate;
    int src_channels = param->in[0].input_channels;
    int src_sample_fmt = param->in[0].input_sample_fmt;   /*??????pcap?????????????????????*/
    int dst_sample_rate = param->output_sample_rate ? param->output_sample_rate : src_sample_rate;
    int dst_channels = param->output_channels ? param->output_channels : src_channels;
    int dst_sample_fmt = param->output_sample_fmt ? param->output_sample_fmt : src_sample_fmt;
    bool_t need_transcoding = FALSE;
    int src_width = param->in[0].input_width;
    int src_height = param->in[0].input_height;
    int dst_width = param->output_width ? param->output_width : src_width;
    int dst_height = param->output_height ? param->output_height : src_height;
    int dst_pix_fmt = param->output_pix_fmt;
    bool_t need_scale = FALSE;

    bool_t need_mix = FALSE;

    for (i = 0; i < param->input_file_count; i++)
    {
        stream->source[i] = ms_factory_create_filter(factory, MS_PARSE_PCAP_ID);
        ms_filter_call_method(stream->source[i], MS_SET_FILE_NAME, (void *)param->in[i].input_file);
        ms_filter_call_method(stream->source[i], MS_SET_SRC_ADDR, (void *)param->in[i].input_src_addr);
        ms_filter_call_method(stream->source[i], MS_SET_DEST_ADDR, (void *)param->in[i].input_dst_addr);
//        ms_filter_set_notify_callback(stream->source[i], pcap_file_end, NULL);
    }







    if (param->input_stream_count > 1)  need_mix = TRUE;
    if (need_mix == FALSE)                                      /*???????????????,??????????????????transcoding / scale*/
    {
        if (param->output_sample_rate && param->output_sample_rate != param->in[0].input_sample_rate)
            need_transcoding = TRUE;
        if (param->output_channels && param->output_channels != param->in[0].input_channels)
            need_transcoding = TRUE;
        if (param->output_mime_type && strcasecmp(param->output_mime_type, param->in[0].input_mime_type) != 0)
            need_transcoding = TRUE;

        if (param->output_width != param->in[0].input_width)      need_scale = TRUE;
        if (param->output_height != param->in[0].input_height)    need_scale = TRUE;
    }






    if (need_mix || need_transcoding)
    {
        for (i = 0; i < param->input_stream_count; i++)
        {
            stream->audio.decoder[i] = ms_factory_create_decoder(factory, param->in[i].input_mime_type);
            if (stream->audio.decoder[i])
            {
#if 0
                ms_filter_call_method(stream->audio.decoder[i], MS_GET_SAMPLE_RATE, &src_sample_rate);
                ms_filter_call_method(stream->audio.decoder[i], MS_GET_CHANNELS, &src_channels);
                ms_filter_call_method(stream->audio.decoder[i], MS_GET_SAMPLE_FMT, &src_sample_fmt);     /*????????????????????????,????????????*/
                stream->sample_rate = src_sample_rate ? src_sample_rate : 44100;
                stream->channels = src_channels ? src_channels : 1;
                stream->sample_fmt = src_sample_fmt ? src_sample_fmt : AV_SAMPLE_FMT_FLTP;            /*???????????????????????????,????????????????????????,???????????????????????????*/
#endif
            }
        }


        stream->audio.amix = ms_factory_create_filter(factory, MS_AMIX_ID);
        if (stream->audio.amix)
        {
            int pos = 0;
            char args[512] = {0};
            /*??????amix?????????*/
            pos = snprintf(args, sizeof(args), "inputs=%d", param->input_stream_count);
            for (i = 0; i < param->input_stream_count; i++)
            {
                pos += snprintf(args+pos, sizeof(args), ":sample_rate=%d:channels=%d:sample_fmt=%d",
                            param->in[i].input_sample_rate, param->in[i].input_channels, param->in[i].input_sample_fmt);
            }
            printf("%s : args = [%s]\n", __func__, args);
            ms_filter_call_method(stream->audio.amix, MS_SET_AMIX_INFO, args);
            ms_filter_call_method(stream->audio.amix, MS_SET_OUTPUT_SAMPLE_RATE, (void *)&dst_sample_rate);
            ms_filter_call_method(stream->audio.amix, MS_SET_OUTPUT_CHANNELS, (void *)&dst_channels);
            ms_filter_call_method(stream->audio.amix, MS_SET_OUTPUT_SAMPLE_FMT, &dst_sample_fmt);
        }


        stream->audio.encoder = ms_factory_create_encoder(factory, param->output_mime_type ? param->output_mime_type : DEFAULT_MIME_TYPE);
        if (stream->audio.encoder)
        {
            ms_filter_call_method(stream->audio.encoder, MS_SET_SAMPLE_RATE, (void *)&dst_sample_rate);
            ms_filter_call_method(stream->audio.encoder, MS_SET_CHANNELS, (void *)&dst_channels);
            ms_filter_call_method(stream->audio.encoder, MS_SET_SAMPLE_FMT, (void *)&dst_sample_fmt);     /*???????????????format???????????????,????????????????????????????????????,?????????*/
        }

//        stream->audio.resample = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
//        stream_configure_resampler(   stream, stream->audio.resample, stream->audio.decoder, stream->audio.encoder);
    }



    if (need_mix || need_scale)
    {
        for (i = 0; i < param->input_stream_count; i++)
        {
        stream->video.decoder[i] = ms_factory_create_decoder(factory, "H264");
//        stream->width = src_width ? src_width : 1920;
//        stream->height = src_height ? src_height : 1080;
        }


        stream->video.vmix = ms_factory_create_filter(factory, MS_VMIX_ID);
        if (stream->video.vmix)
        {
            int pos = 0;
            char args[512] = {0};
            /*??????amix?????????*/
            pos = snprintf(args, sizeof(args), "inputs=%d", param->input_stream_count);
            for (i = 0; i < param->input_stream_count; i++)
            {
                pos += snprintf(args+pos, sizeof(args), ":width=%d:height=%d:pix_fmt=%d",
                            param->in[i].input_width, param->in[i].input_height, param->in[i].input_pix_fmt);
            }
            ms_filter_call_method(stream->video.vmix, MS_SET_VMIX_INFO, args);
            ms_filter_call_method(stream->video.vmix, MS_SET_OUTPUT_WIDTH, (void *)&dst_width);
            ms_filter_call_method(stream->video.vmix, MS_SET_OUTPUT_HEIGTH, (void *)&dst_height);
            ms_filter_call_method(stream->video.vmix, MS_SET_OUTPUT_PIX_FMT, &dst_pix_fmt);
            ms_filter_set_notify_callback(stream->video.vmix, pcap_file_end, NULL);
        }


        stream->video.encoder = ms_factory_create_encoder(factory, "H264");
        if (stream->video.encoder)
        {
            ms_filter_call_method(stream->video.encoder, MS_SET_WIDTH, &dst_width);
            ms_filter_call_method(stream->video.encoder, MS_SET_HEIGTH, &dst_height);
        }

//        stream->video.scale = ms_factory_create_filter(factory, MS_SCALE_ID);
//        stream_configure_scale(stream, stream->video.scale, stream->video.decoder, stream->video.encoder);
    }

    for (i = 0; i < param->input_stream_count; i++)
    {
        stream->video.regroup[i] = ms_factory_create_filter(factory, MS_H264_REGROUP_ID);
    }

    stream->muxer = ms_factory_create_filter(factory, MS_MUXER_ID);
    if (stream->muxer)
    {
        ms_filter_call_method(stream->muxer, MS_SET_FILE_NAME, (void *)param->output_file);

        ms_filter_call_method(stream->muxer, MS_SET_WIDTH, (void *)&dst_width);
        ms_filter_call_method(stream->muxer, MS_SET_HEIGTH, (void *)&dst_height);

        ms_filter_call_method(stream->muxer, MS_SET_SAMPLE_RATE, (void *)&dst_sample_rate);
        ms_filter_call_method(stream->muxer, MS_SET_CHANNELS, (void *)&dst_channels);
        ms_filter_call_method(stream->muxer, MS_SET_SAMPLE_FMT, (void *)&dst_sample_fmt);
        ms_filter_call_method(stream->muxer, MS_SET_MIME_TYPE, (void *)(param->output_mime_type));
    }



    for (i = 0; i < param->input_stream_count; i++)
    {
        ms_connection_helper_start(&h);
        ms_connection_helper_link(&h, stream->source[i], -1, 0);
        if (stream->audio.decoder[i])   ms_connection_helper_link(&h, stream->audio.decoder[i], 0, 0);
        if (stream->audio.amix)   ms_connection_helper_link(&h, stream->audio.amix, i, 0);

        ms_connection_helper_start(&h);
        ms_connection_helper_link(&h, stream->source[i], -1, 1);
        ms_connection_helper_link(&h, stream->video.regroup[i], 0, 0);
        if (stream->video.decoder[i])   ms_connection_helper_link(&h, stream->video.decoder[i], 0, 0);
        if (stream->video.vmix)   ms_connection_helper_link(&h, stream->video.vmix, i, 0);
    }

    ms_connection_helper_start(&h);
    if (stream->audio.amix)   ms_connection_helper_link(&h, stream->audio.amix, -1, 0);
    if (stream->audio.encoder)   ms_connection_helper_link(&h, stream->audio.encoder, 0, 0);
    ms_connection_helper_link(&h, stream->muxer, 0, -1);


    ms_connection_helper_start(&h);
    if (stream->video.vmix)   ms_connection_helper_link(&h, stream->video.vmix, -1, 0);
    if (stream->video.encoder)   ms_connection_helper_link(&h, stream->video.encoder, 0, 0);
    ms_connection_helper_link(&h, stream->muxer, 1, -1);



    stream->ticker = ms_ticker_new();
    ticker_attach(stream->ticker, stream);
}

#if 0
static int pcap_stream_stop(MSFactory *factory, PcapStream *stream)
{
    MSConnectionHelper h;

    ms_connection_helper_start(&h);
    ms_connection_helper_unlink(&h, stream->source, -1, 0);
    if (stream->audio.decoder)  ms_connection_helper_unlink(&h, stream->audio.decoder, 0, 0);
    if (stream->audio.resample)   ms_connection_helper_unlink(&h, stream->audio.resample, 0, 0);
    if (stream->audio.encoder)  ms_connection_helper_unlink(&h, stream->audio.encoder, 0, 0);
    ms_connection_helper_unlink(&h, stream->muxer, 0, -1);
    ms_connection_helper_start(&h);
    ms_connection_helper_unlink(&h, stream->source, -1, 1);
//    ms_connection_helper_unlink(&h, stream->video.regroup, 0, 0);
    if (stream->video.decoder)   ms_connection_helper_unlink(&h, stream->video.decoder, 0, 0);
    if (stream->video.scale)     ms_connection_helper_unlink(&h, stream->video.scale, 0, 0);
    if (stream->video.encoder)   ms_connection_helper_unlink(&h, stream->video.encoder, 0, 0);
    ms_connection_helper_unlink(&h, stream->muxer, 1, -1);


    if (stream->source)     ms_filter_destroy(stream->source);
    if (stream->audio.decoder)    ms_filter_destroy(stream->audio.decoder);
    if (stream->audio.resample)   ms_filter_destroy(stream->audio.resample);
    if (stream->audio.encoder)    ms_filter_destroy(stream->audio.encoder);
//    if (stream->video.regroup)    ms_filter_destroy(stream->video.regroup);
    if (stream->video.decoder)    ms_filter_destroy(stream->video.decoder);
    if (stream->video.scale)    ms_filter_destroy(stream->video.scale);
    if (stream->video.encoder)    ms_filter_destroy(stream->video.encoder);
    if (stream->muxer)      ms_filter_destroy(stream->muxer);
}
#endif


void print_options(Parameter *param)
{
    int i;
    for (i = 0; i < param->input_stream_count; i++)
    {
        printf("input[%d] file = [%s]\n", i, param->in[i].input_file ? param->in[i].input_file : "NULL");
        printf("input[%d] src_ip_addr = [%s]\n", i, param->in[i].input_src_addr ? param->in[i].input_src_addr : "NULL");
        printf("input[%d] dst_ip_addr = [%s]\n", i, param->in[i].input_dst_addr ? param->in[i].input_dst_addr : "NULL");
        printf("input[%d] sample_rate = [%d]\n", i, param->in[i].input_sample_rate);
        printf("input[%d] channels = [%d]\n", i, param->in[i].input_channels);
        printf("input[%d] sample_fmt = [%d]\n", i, param->in[i].input_sample_fmt);
        printf("input[%d] mime_type = [%s]\n", i, param->in[i].input_mime_type ? param->in[i].input_mime_type : "NULL");
        printf("input[%d] width = [%d] : height = [%d]\n", i, param->in[i].input_width, param->in[i].input_height);
        printf("input[%d] pix_fmt = [%d]\n", i, param->in[i].input_pix_fmt);
    }
    printf("input_file_count = [%d] : input_stream_count = [%d]\n", param->input_file_count, param->input_stream_count);
    printf("output file = [%s]\n", param->output_file ? param->output_file : "NULL");
    printf("output sample_rate = [%d]\n", param->output_sample_rate);
    printf("output channels = [%d]\n", param->output_channels);
    printf("output sample_fmt = [%d]\n", param->output_sample_fmt);
    printf("output mime_type = [%s]\n", param->output_mime_type ? param->output_mime_type : "NULL");
    printf("output width = [%d] : height = [%d]\n", param->output_width, param->output_height);
    printf("output pix_fmt = [%d]\n", param->output_pix_fmt);
}

int main(int argc, const char *argv[])
{
    char ch = 0;
    MSFactory *factory = NULL;
    Parameter param;
    PcapStream stream;
    memset(&param, 0, sizeof(Parameter));
    memset(&stream, 0, sizeof(PcapStream));

    parse_options(argc,  argv, &param);
    print_options(&param);

#if 0
    if (param.input_file == NULL || param.output_file == NULL || param.input_addr == NULL
        || param.output_addr == NULL ||  param.input_mime_type == NULL)
    {
        print_usage();
        return -1;
    }
#endif

    factory = ms_factory_new();
    if (factory == NULL)
    {
        printf("ms_factory_new failed.\n");
        return -1;
    }

    pcap_stream_start_from_param(factory, &stream, &param);

    while (1)
    {
        scanf("%c", &ch);
        if (ch == 'q' || ch == 'Q')
        {
            printf("\n[%c : quit.]\n\n", ch);
            break;
        }
    }

//    pcap_stream_stop(factory, &stream);
    if (stream.muxer)      ms_filter_destroy(stream.muxer);
    ms_factory_destroy(factory);
    return 0;
}



