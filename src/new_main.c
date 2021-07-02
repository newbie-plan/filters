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

typedef struct Parameter
{
    char *  input_file;
    char *  input_addr;
    int     input_sample_rate;
    int     input_channels;
    int     input_sample_fmt;
    char *  input_mime_type;
    int     input_width;
    int     input_heigth;
    char *  output_file;
    char *  output_addr;
    int     output_sample_rate;
    int     output_channels;
    int     output_sample_fmt;
    char *  output_mime_type;
    int     output_width;
    int     output_heigth;
}Parameter;

typedef struct PcapStream
{
    MSTicker *ticker;
    MSFilter *source;
    struct Audio
    {
        MSFilter *decoder;
        MSFilter *resample;
        MSFilter *encoder;
    }audio;
    struct Video
    {
        MSFilter *regroup;
        MSFilter *decoder;
        MSFilter *scale;
        MSFilter *encoder;
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
    {"addr",        required_argument,  NULL, 'd' },
    {"sample_rate", required_argument,  NULL, 'r' },
    {"channels",    required_argument,  NULL, 'c' },
    {"format",      required_argument,  NULL, 'f' },
    {"acodec",      required_argument,  NULL, 'a' },
    {"size",        required_argument,  NULL, 's' },
    {"help",        no_argument,        NULL, 'h' },
    {0,             0,                  0,     0  }
};

static void print_usage()
{
    printf("Option:\n");
    printf("The option marked with * is required.\n");
    printf(" -i, --in=FILE       *       The file name for input file.\n");
    printf(" -o, --out=FILE      *       The file name for output file.\n");
    printf(" -d, --addr=IP       *       The address to be filtered.\n");
    printf(" -r, --sample_rate=          The sample rate of audio.\n");
    printf(" -c, --channels=             The channels rate of audio.\n");
    printf(" -f, --format,       *       The format of audio, eg: s16p/fltp.\n");
    printf(" -a, --acodec,       *       The codec of audio.\n");
    printf(" -s, --size,                 The resolution of video.\n");
    printf(" -h, --help                  Print this message and exit.\n");
}

static void parse_options(int argc, const char *argv[], Parameter *param)
{
    int optc = -1;
    int opt_index = -1;
    int sample_rate = 0;
    int channels = 0;
    int sample_fmt = NULL;
    char *mime_type = NULL;
    char *ip_addr = NULL;
    int width = 0;
    int height = 0;
    

    while ((optc = getopt_long(argc, (char *const *)argv, "hi:o:d:r:c:f:a:s:", long_options, &opt_index)) != -1)
    {
        switch (optc)
        {
            case 'i':
            {
                param->input_file = optarg;
                param->input_addr = ip_addr;
                param->input_sample_rate = sample_rate;
                param->input_channels = channels;
                param->input_sample_fmt = sample_fmt;
                param->input_mime_type = mime_type;
                param->input_width = width;
                param->input_heigth = height;

                sample_rate = 0;
                channels = 0;
                sample_fmt = 0;
                mime_type = NULL;
                ip_addr = NULL;
                width = 0;
                height = 0;
                break;
            }
            case 'o':
            {
                param->output_file = optarg;
                param->output_addr = ip_addr;
                param->output_sample_rate = sample_rate;
                param->output_channels = channels;
                param->output_sample_fmt = sample_fmt;
                param->output_mime_type = mime_type;
                param->output_width = width;
                param->output_heigth = height;

                sample_rate = 0;
                channels = 0;
                sample_fmt = 0;
                mime_type = NULL;
                ip_addr = NULL;
                width = 0;
                height = 0;
                break;
            }
            case 'd':
            {
                ip_addr = optarg;
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
                if (strncasecmp(optarg, "fltp", sizeof("fltp")) == 0)
                {
                    sample_fmt = AV_SAMPLE_FMT_FLTP;
                }
                else if (strncasecmp(optarg, "s16p", sizeof("s16p")) == 0)
                {
                    sample_fmt = AV_SAMPLE_FMT_S16P;
                }
                else if (strncasecmp(optarg, "s16", sizeof("s16")) == 0)
                {
                    sample_fmt = AV_SAMPLE_FMT_S16;
                }
                else
                {
                    sample_fmt = AV_SAMPLE_FMT_FLTP;
                }
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

    filters = bctbx_list_append(filters, stream->source);
    if (stream->audio.decoder)  filters = bctbx_list_append(filters, stream->audio.decoder);
    if (stream->audio.resample)   filters = bctbx_list_append(filters, stream->audio.resample);
    if (stream->audio.encoder)  filters = bctbx_list_append(filters, stream->audio.encoder);
    filters = bctbx_list_append(filters, stream->video.regroup);
    if (stream->video.decoder)  filters = bctbx_list_append(filters, stream->video.decoder);
    if (stream->video.scale)    filters = bctbx_list_append(filters, stream->video.scale);
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
    int ret = -1;
    int src_sample_rate = param->input_sample_rate;
    int src_channels = param->input_channels;
    int src_sample_fmt = param->input_sample_fmt;   /*要从pcap文件中解析出来*/
    int dst_sample_rate = param->output_sample_rate ? param->output_sample_rate : src_sample_rate;
    int dst_channels = param->output_channels ? param->output_channels : src_channels;
    int dst_sample_fmt = -1;
    bool_t need_transcoding = FALSE;
    int src_width = param->input_width;
    int src_height = param->input_heigth;
    int dst_width = param->output_width ? param->output_width : src_width;
    int dst_height = param->output_heigth ? param->output_heigth : src_height;
    bool_t need_scale = FALSE;

    stream->source = ms_factory_create_filter(factory, MS_PARSE_PCAP_ID);
    ms_filter_call_method(stream->source, MS_SET_FILE_NAME, (void *)param->input_file);
    ms_filter_call_method(stream->source, MS_SET_SRC_ADDR, (void *)param->input_addr);
    ms_filter_call_method(stream->source, MS_SET_DEST_ADDR, (void *)param->output_addr);
    ms_filter_set_notify_callback(stream->source, pcap_file_end, NULL);


    if (param->output_sample_rate && param->output_sample_rate != src_sample_rate)
        need_transcoding = TRUE;
    if (param->output_channels && param->output_channels != src_channels)
        need_transcoding = TRUE;
    if (param->output_mime_type && strcasecmp(param->output_mime_type, param->input_mime_type) != 0)
        need_transcoding = TRUE;

    if (need_transcoding)
    {
        stream->audio.decoder = ms_factory_create_decoder(factory, param->input_mime_type);
        if (stream->audio.decoder)
        {
            ms_filter_call_method(stream->audio.decoder, MS_GET_SAMPLE_RATE, &src_sample_rate);
            ms_filter_call_method(stream->audio.decoder, MS_GET_CHANNELS, &src_channels);
            ms_filter_call_method(stream->audio.decoder, MS_GET_SAMPLE_FMT, &src_sample_fmt);     /*某些格式是固定的,能获取到*/
            stream->sample_rate = src_sample_rate ? src_sample_rate : 44100;
            stream->channels = src_channels ? src_channels : 1;
            stream->sample_fmt = src_sample_fmt ? src_sample_fmt : AV_SAMPLE_FMT_FLTP;            /*某些格式不是固定的,解码之后才能知道,这就需要提前探测出*/
        }

        stream->audio.encoder = ms_factory_create_encoder(factory, param->output_mime_type ? param->output_mime_type : param->input_mime_type);
        if (stream->audio.encoder)
        {
            ms_filter_call_method(stream->audio.encoder, MS_SET_SAMPLE_RATE, (void *)&dst_sample_rate);
            ms_filter_call_method(stream->audio.encoder, MS_SET_CHANNELS, (void *)&dst_channels);
            ms_filter_call_method(stream->audio.encoder, MS_GET_SAMPLE_FMT, &dst_sample_fmt);     /*编码格式的format都是固定的,如果解码之后的数据不匹配,重采样*/
        }

        stream->audio.resample = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
        stream_configure_resampler(   stream, stream->audio.resample, stream->audio.decoder, stream->audio.encoder);
    }

    if (param->output_width != param->input_width)      need_scale = TRUE;
    if (param->output_heigth != param->input_heigth)    need_scale = TRUE;

    if (need_scale)
    {
        stream->video.decoder = ms_factory_create_decoder(factory, "H264");
        stream->width = src_width ? src_width : 1920;
        stream->height = src_height ? src_height : 1080;

        stream->video.encoder = ms_factory_create_encoder(factory, "H264");
        if (stream->video.encoder)
        {
            ms_filter_call_method(stream->video.encoder, MS_SET_WIDTH, &dst_width);
            ms_filter_call_method(stream->video.encoder, MS_SET_HEIGTH, &dst_height);
        }

        stream->video.scale = ms_factory_create_filter(factory, MS_SCALE_ID);
        stream_configure_scale(stream, stream->video.scale, stream->video.decoder, stream->video.encoder);
    }

    stream->video.regroup = ms_factory_create_filter(factory, MS_H264_REGROUP_ID);
    stream->muxer = ms_factory_create_filter(factory, MS_MUXER_ID);
    if (stream->muxer)
    {
        ms_filter_call_method(stream->muxer, MS_SET_FILE_NAME, (void *)param->output_file);

        ms_filter_call_method(stream->muxer, MS_SET_WIDTH, (void *)&dst_width);
        ms_filter_call_method(stream->muxer, MS_SET_HEIGTH, (void *)&dst_height);

        ms_filter_call_method(stream->muxer, MS_SET_SAMPLE_RATE, (void *)&dst_sample_rate);
        ms_filter_call_method(stream->muxer, MS_SET_CHANNELS, (void *)&dst_channels);
        ms_filter_call_method(stream->muxer, MS_SET_SAMPLE_FMT, (void *)&dst_sample_fmt);
        ms_filter_call_method(stream->muxer, MS_SET_MIME_TYPE, (void *)(param->output_mime_type ? param->output_mime_type : param->input_mime_type));
    }


    ms_connection_helper_start(&h);
    ms_connection_helper_link(&h, stream->source, -1, 0);
    if (stream->audio.decoder)   ms_connection_helper_link(&h, stream->audio.decoder, 0, 0);
    if (stream->audio.resample)   ms_connection_helper_link(&h, stream->audio.resample, 0, 0);
    if (stream->audio.encoder)   ms_connection_helper_link(&h, stream->audio.encoder, 0, 0);
    ms_connection_helper_link(&h, stream->muxer, 0, -1);
    ms_connection_helper_start(&h);
    ms_connection_helper_link(&h, stream->source, -1, 1);
    ms_connection_helper_link(&h, stream->video.regroup, 0, 0);
    if (stream->video.decoder)   ms_connection_helper_link(&h, stream->video.decoder, 0, 0);
    if (stream->video.scale)     ms_connection_helper_link(&h, stream->video.scale, 0, 0);
    if (stream->video.encoder)   ms_connection_helper_link(&h, stream->video.encoder, 0, 0);
    ms_connection_helper_link(&h, stream->muxer, 1, -1);

    stream->ticker = ms_ticker_new();
    ticker_attach(stream->ticker, stream);
}


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
    ms_connection_helper_unlink(&h, stream->video.regroup, 0, 0);
    if (stream->video.decoder)   ms_connection_helper_unlink(&h, stream->video.decoder, 0, 0);
    if (stream->video.scale)     ms_connection_helper_unlink(&h, stream->video.scale, 0, 0);
    if (stream->video.encoder)   ms_connection_helper_unlink(&h, stream->video.encoder, 0, 0);
    ms_connection_helper_unlink(&h, stream->muxer, 1, -1);


    if (stream->source)     ms_filter_destroy(stream->source);
    if (stream->audio.decoder)    ms_filter_destroy(stream->audio.decoder);
    if (stream->audio.resample)   ms_filter_destroy(stream->audio.resample);
    if (stream->audio.encoder)    ms_filter_destroy(stream->audio.encoder);
    if (stream->video.regroup)    ms_filter_destroy(stream->video.regroup);
    if (stream->video.decoder)    ms_filter_destroy(stream->video.decoder);
    if (stream->video.scale)    ms_filter_destroy(stream->video.scale);
    if (stream->video.encoder)    ms_filter_destroy(stream->video.encoder);
    if (stream->muxer)      ms_filter_destroy(stream->muxer);
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
    printf("input file = [%s]\n", param.input_file ? param.input_file : "NULL");
    printf("input ip_addr = [%s]\n", param.input_addr ? param.input_addr : "NULL");
    printf("input sample_rate = [%d]\n", param.input_sample_rate);
    printf("input channels = [%d]\n", param.input_channels);
    printf("input sample_fmt = [%d]\n", param.input_sample_fmt);
    printf("input mime_type = [%s]\n", param.input_mime_type ? param.input_mime_type : "NULL");
    printf("input width = [%d] : height = [%d]\n", param.input_width, param.input_heigth);
    printf("output file = [%s]\n", param.output_file ? param.output_file : "NULL");
    printf("output ip_addr = [%s]\n", param.output_addr ? param.output_addr : "NULL");
    printf("output sample_rate = [%d]\n", param.output_sample_rate);
    printf("output channels = [%d]\n", param.output_channels);
    printf("output sample_fmt = [%d]\n", param.output_sample_fmt);
    printf("output mime_type = [%s]\n", param.output_mime_type ? param.output_mime_type : "NULL");
    printf("output width = [%d] : height = [%d]\n", param.output_width, param.output_heigth);

    if (param.input_file == NULL || param.output_file == NULL || param.input_addr == NULL
        || param.output_addr == NULL ||  param.input_mime_type == NULL)
    {
        print_usage();
        return -1;
    }


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

    pcap_stream_stop(factory, &stream);
    ms_factory_destroy(factory);
    return 0;
}



