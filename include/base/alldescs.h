#include <base/msfilter.h>


extern MSFilterDesc ms_parse_pcap_desc;
extern MSFilterDesc ms_g711_dec_desc;
extern MSFilterDesc ms_mp3_dec_desc;
extern MSFilterDesc ms_mp3_enc_desc;
extern MSFilterDesc ms_aac_enc_desc;
extern MSFilterDesc ms_resample_desc;
extern MSFilterDesc ms_h264_regroup_desc;
extern MSFilterDesc ms_h264_enc_desc;
extern MSFilterDesc ms_h264_dec_desc;
extern MSFilterDesc ms_scale_desc;
extern MSFilterDesc ms_muxer_desc;
extern MSFilterDesc ms_vmix_desc;
extern MSFilterDesc ms_amix_desc;



MSFilterDesc *ms_filter_descs[] = 
{
    &ms_parse_pcap_desc,
    &ms_g711_dec_desc,
    &ms_mp3_dec_desc,
    &ms_mp3_enc_desc,
    &ms_aac_enc_desc,
    &ms_resample_desc,
    &ms_h264_regroup_desc,
    &ms_h264_enc_desc,
    &ms_h264_dec_desc,
    &ms_scale_desc,
    &ms_muxer_desc,
    &ms_vmix_desc,
    &ms_amix_desc,
    NULL
};


