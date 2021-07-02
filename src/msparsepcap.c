#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <base/mscommon.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>

typedef struct PcapHeader
{
    uint32_t magic;
    uint16_t major;
    uint16_t minor;
    uint32_t time_zone;
    uint32_t sig_figs;
    uint32_t snap_len;
    uint32_t link_type;
}PcapHeader;

struct time_val
{
    uint32_t tv_sec;
    uint32_t tv_usec;
};

typedef struct PacketHeader
{
    struct time_val timestamp;
    uint32_t cap_len;
    uint32_t data_len;
}PacketHeader;

typedef struct EthernetHeader
{
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
}EthernetHeader;

typedef struct IPHeader
{
    uint8_t version : 4;
    uint8_t header_len : 4;
    uint8_t service_field;
    uint16_t total_len;
    uint16_t identifi;
    uint16_t flags : 3;
    uint16_t offset : 13;
    uint8_t time2live;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_addr;
    uint32_t dest_addr;
}IPHeader;

typedef struct UdpHeader
{
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
}UdpHeader;

typedef struct RtpHeader
{
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload_type : 7;
    uint8_t marker : 1;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
}RtpHeader;

typedef struct MediaPacket
{
    struct time_val timestamp;
    char marker;
    int payload_type;
    mblk_t *payload;
    uint32_t need_skip_len;
}MediaPacket;

typedef struct ParsePcapData
{
    FILE *fp;
    int packet_count;
    MSBufferizer pcap_data;
    uint32_t src_addr;
    uint32_t dest_addr;
    struct time_val first_time_audio;    
    struct time_val first_time_video;
}ParsePcapData;

#define BUFFER_SIZE 1024000
#define MIN_SIZE    2048
#define RFC_1889_VERSION 2
#define PACKET_HDR_LEN      sizeof(PacketHeader)
#define ETHERNET_HDR_LEN    sizeof(EthernetHeader)
#define IP_HDR_LEN          sizeof(IPHeader)
#define UDP_HDR_LEN         sizeof(UdpHeader)

static int payload_type[] = {0, 14, 96, 97};
static uint32_t last_packet_seq = -1;



static int fill_bufferizer(FILE *fp, MSBufferizer *pcap_date, int size)
{
    int ret = -1;
    mblk_t *m = NULL;

    if (fp == NULL || pcap_date == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    if (feof(fp) == 0)
    {
        m = allocb(size, 0);
        ret = fread(m->b_wptr, 1, size, fp);
        if (ret > 0)
        {
            m->b_wptr += ret;
            ms_bufferizer_put(pcap_date, m);
        }
        else
        {
            freemsg(m);
        }
    }
    return ret;
}




static void parse_pcap_init(MSFilter *f)
{
    ParsePcapData *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    d = ms_new0(ParsePcapData, 1);
    memset(d, 0, sizeof(ParsePcapData));
    ms_bufferizer_init(&d->pcap_data);
    f->data = (void *)d;
    return;
}

static void parse_pcap_preprocess(MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static uint32_t find_udp_payload(ParsePcapData *d, MediaPacket *pkt)
{
    uint32_t payload_size = 0;
    uint32_t skip_len = 0;    
    PacketHeader pack_h;
    IPHeader ip_h;
    UdpHeader udp_h;
    int total_header_len = PACKET_HDR_LEN + ETHERNET_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN;

    if (ms_bufferizer_get_avail(&d->pcap_data) >= total_header_len)
    {
        memset(&pack_h, 0, sizeof(PacketHeader));
        ms_bufferizer_read(&d->pcap_data, (uint8_t *)&pack_h, sizeof(PacketHeader));
        skip_len = pack_h.cap_len;
        d->packet_count++;

        ms_bufferizer_skip_bytes(&d->pcap_data, sizeof(EthernetHeader));            /*跳过Ethernet header*/
        skip_len -= sizeof(EthernetHeader);

        memset(&ip_h, 0, sizeof(IPHeader));
        ms_bufferizer_read(&d->pcap_data, (uint8_t *)&ip_h, sizeof(IPHeader));
        skip_len -= sizeof(IPHeader);

        if (ip_h.protocol == 17 && ip_h.src_addr == d->src_addr && ip_h.dest_addr == d->dest_addr)
        {
            memset(&udp_h, 0, sizeof(UdpHeader));
            ms_bufferizer_read(&d->pcap_data, (uint8_t *)&udp_h, sizeof(UdpHeader));
            skip_len -= sizeof(UdpHeader);

            pkt->timestamp.tv_sec = pack_h.timestamp.tv_sec;
            pkt->timestamp.tv_usec = pack_h.timestamp.tv_usec;
            payload_size = ntohs(udp_h.length) - sizeof(UdpHeader);
        }
    }

    pkt->need_skip_len = skip_len;
    return payload_size;
}


static int read_one_rtp_packet(ParsePcapData *d, MediaPacket *pkt)
{
    mblk_t *m = NULL;
    int payload_type = -1;
    uint32_t skip_len = 0;
    uint32_t udp_payload_size = 0;

    udp_payload_size = find_udp_payload(d, pkt);
    skip_len = pkt->need_skip_len;
    if (udp_payload_size > 0)
    {
        if (ms_bufferizer_get_avail(&d->pcap_data) >= udp_payload_size)
        {
            RtpHeader rtp_h;
            memset(&rtp_h, 0, sizeof(RtpHeader));
            ms_bufferizer_read(&d->pcap_data, (uint8_t *)&rtp_h, sizeof(RtpHeader));
            skip_len -= sizeof(RtpHeader);

            if (rtp_h.version == RFC_1889_VERSION && rtp_h.seq != last_packet_seq)
            {
                uint32_t rtp_size = 0;

                rtp_size = udp_payload_size;
                rtp_size -= sizeof(RtpHeader);
                rtp_size -= 4 * rtp_h.csrc_count;

                m = allocb(rtp_size, 0);
                ms_bufferizer_read(&d->pcap_data, m->b_wptr, rtp_size);
                m->b_wptr += rtp_size;
                mblk_set_marker_info(m, rtp_h.marker);

                pkt->marker = rtp_h.marker;
                payload_type = rtp_h.payload_type;
                skip_len -= rtp_size;
                last_packet_seq = rtp_h.seq;
            }
        }
    }

    if (skip_len > 0)
    {
        ms_bufferizer_skip_bytes(&d->pcap_data, skip_len);
    }

    pkt->payload = m;
    return payload_type;
}

static void parse_pcap_process(MSFilter *f)
{
    ParsePcapData *d = NULL;
    int payload_type = -1;
    uint32_t pts = 0;
    MediaPacket pkt;
    memset(&pkt, 0, sizeof(MediaPacket));

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ParsePcapData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    do
    {
        if (ms_bufferizer_get_avail(&d->pcap_data) <= MIN_SIZE)
        {
            fill_bufferizer(d->fp, &d->pcap_data, BUFFER_SIZE);
        }

        if (feof(d->fp) && ms_bufferizer_get_avail(&d->pcap_data)
            < PACKET_HDR_LEN + ETHERNET_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN)
        {
            printf("%s : pcap file is in the end\n", __func__);
            ms_filter_notify_no_arg(f, 0);
            break;
        }
    } while ((payload_type = read_one_rtp_packet(d, &pkt)) < 0);

    switch (payload_type)
    {
        case 0:
        {
            if (d->first_time_audio.tv_sec == 0 && d->first_time_audio.tv_usec == 0)
            {
                d->first_time_audio = pkt.timestamp;
            }

            pts = (pkt.timestamp.tv_sec - d->first_time_audio.tv_sec) * 1000000 + (pkt.timestamp.tv_usec - d->first_time_audio.tv_usec);
            mblk_set_timestamp_info(pkt.payload, pts);

            ms_queue_put(f->outputs[0], pkt.payload);
            break;
        }
        case 14:
        {
            if (d->first_time_audio.tv_sec == 0 && d->first_time_audio.tv_usec == 0)
            {
                d->first_time_audio = pkt.timestamp;
            }

            pts = (pkt.timestamp.tv_sec - d->first_time_audio.tv_sec) * 1000000 + (pkt.timestamp.tv_usec - d->first_time_audio.tv_usec);
            mblk_set_timestamp_info(pkt.payload, pts);

            pkt.payload->b_rptr += 4;

            ms_queue_put(f->outputs[0], pkt.payload);
            break;
        }
        case 96:
        {
            if (pkt.marker)
            {
                if (d->first_time_video.tv_sec == 0 && d->first_time_video.tv_usec == 0)
                {
                    d->first_time_video = pkt.timestamp;
                }

                pts = (pkt.timestamp.tv_sec - d->first_time_video.tv_sec) * 1000000 + (pkt.timestamp.tv_usec - d->first_time_video.tv_usec);
                mblk_set_timestamp_info(pkt.payload, pts);
            }

            ms_queue_put(f->outputs[1], pkt.payload);
            break;
        }
        default:
        {
            if (pkt.payload) freemsg(pkt.payload);
            printf("%s : Unsurport this payload type [%d]\n", __func__, payload_type);
        }
    }

    return;
}

static void parse_pcap_postprocess(MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

static void parse_pcap_uninit(MSFilter *f)
{
    ParsePcapData *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (ParsePcapData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    printf("%s : packet_count = [%d]\n", __func__, d->packet_count);

    if (d->fp != NULL)
    {
        fclose(d->fp);
    }
    ms_bufferizer_flush(&d->pcap_data);
    ms_free(d);
    return;
}



static int open_pcap_file(MSFilter *f, void *arg)
{
    ParsePcapData *d = NULL;
    const char *file_name = (const char *)arg;
    printf("%s : file_name = [%s]\n", __func__, file_name);

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (ParsePcapData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    d->fp = fopen(file_name, "rb");
    if (d->fp == NULL)
    {
        printf("%s : fopen %s failed.\n", __func__, file_name);
        return;
    }
    fseek(d->fp, sizeof(PcapHeader), SEEK_SET);      /*pcap header确定大小端,暂不处理*/
    fill_bufferizer(d->fp, &d->pcap_data, BUFFER_SIZE);
    return 0;
}

static int set_src_addr(MSFilter *f, void *arg)
{
    ParsePcapData *d = NULL;
    const char *src_addr = (const char *)arg;    

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (ParsePcapData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    printf("%s : src_addr = [%s]\n", __func__, src_addr);
    d->src_addr = inet_addr(src_addr);
}

static int set_dest_addr(MSFilter *f, void *arg)
{
    ParsePcapData *d = NULL;
    const char *dest_addr = (const char *)arg;

    if (f == NULL || arg == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }
    d = (ParsePcapData *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return -1;
    }

    printf("%s : dest_addr = [%s]\n", __func__, dest_addr);
    d->dest_addr = inet_addr(dest_addr);
}


static int probe_input_format(MSFilter *f, void *arg)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
}


MSFilterMethod parse_pcap_methods[] = {
    {MS_SET_FILE_NAME, open_pcap_file},
    {MS_SET_SRC_ADDR, set_src_addr},
    {MS_SET_DEST_ADDR, set_dest_addr},
    {MS_PROBE_INPUT_FORMAT, probe_input_format},
    {-1, NULL},
};




MSFilterDesc ms_parse_pcap_desc = {
    .id = MS_PARSE_PCAP_ID,
    .name = "ParsePcap",
    .text = "parse pcap file",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 0,
    .noutputs = 2,
    .init = parse_pcap_init,
    .preprocess = parse_pcap_preprocess,
    .process = parse_pcap_process,
    .postprocess = parse_pcap_postprocess,
    .uninit = parse_pcap_uninit,
    .methods = parse_pcap_methods,
};


