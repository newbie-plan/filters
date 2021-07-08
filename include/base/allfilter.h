#ifndef __ALLFILTER_H__
#define __ALLFILTER_H__

typedef enum MSFilterId
{
    MS_PARSE_PCAP_ID,
    MS_AAC_DEC_ID,
    MS_AAC_ENC_ID,
    MS_MP3_DEC_ID,
    MS_MP3_ENC_ID,
    MS_G711_DEC_ID,
    MS_H264_REGROUP_ID,
    MS_H264_DEC_ID,
    MS_H264_ENC_ID,
    MS_MUXER_ID,
    MS_RESAMPLE_ID,
    MS_SCALE_ID,
    MS_AMIX_ID,
    MS_VMIX_ID,
}MSFilterId;

#endif


