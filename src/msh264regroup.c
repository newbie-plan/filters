#include <stdio.h>
#include <string.h>
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <base/msqueue.h>



typedef struct
{
    mblk_t *frame;
}H264Regroup_t;

static uint8_t start_code_1[] = {0x00, 0x00, 0x00, 0x01};
static uint8_t start_code_2[] = {0x00, 0x00, 0x01};


void h264_regroup_init(struct _MSFilter *f)
{
    H264Regroup_t *d = NULL;
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

    d = ms_new0(H264Regroup_t, 1);
    memset(d, 0, sizeof(H264Regroup_t));

    f->data = (void *)d;
}

void h264_regroup_preprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}



static int h264_packet_fu_a(uint8_t *nal, uint32_t nal_len, mblk_t **om)
{
    mblk_t *m = NULL;
    uint32_t total_len = 0;
    uint8_t fu_indicator, fu_header, start_bit, nal_type, nal_header;

    if (nal_len < 3)
    {
        printf("Too short data for FU-A H.264 RTP packet\n");
        return -1;
    }

    fu_indicator = nal[0];
    fu_header    = nal[1];
    start_bit    = fu_header >> 7;
    nal_type     = fu_header & 0x1f;
    nal_header   = fu_indicator & 0xe0 | nal_type;

    nal += 2;
    nal_len -= 2;

    total_len = nal_len;
    if (start_bit) total_len += sizeof(start_code_1) + 1;
    m = allocb(total_len, 0);
    if (start_bit)
    {
        memcpy(m->b_wptr, start_code_1, sizeof(start_code_1));
        m->b_wptr += sizeof(start_code_1);
        memcpy(m->b_wptr, &nal_header, 1);
        m->b_wptr += 1;
    }
    memcpy(m->b_wptr, nal, nal_len);
    m->b_wptr += nal_len;

    *om = m;
    return 0;
}



void h264_regroup_process(struct _MSFilter *f)
{
    H264Regroup_t *d = NULL;
    mblk_t *im = NULL;


    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Regroup_t *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    while ((im = ms_queue_get(f->inputs[0])) != NULL)
    {
        uint32_t nal_len = im->b_wptr - im->b_rptr;
        uint8_t *nal = im->b_rptr;
        uint8_t type = nal[0] & 0x1F;
        uint8_t marker = mblk_get_marker_info(im);
        uint32_t timestamp = 0;


        if (type >= 1 && type <= 23) type = 1;
        switch (type)
        {
            case 1:
            {
                mblk_t *om = NULL;
                om = allocb(nal_len + sizeof(start_code_1), 0);
                memcpy(om->b_wptr, start_code_1, sizeof(start_code_1));
                om->b_wptr += sizeof(start_code_1);
                memcpy(om->b_wptr, nal, nal_len);
                om->b_wptr += nal_len;

                if (d->frame == NULL)
                {
                    d->frame = om;
                }
                else
                {
                    mblk_t *m = NULL;
                    for (m = d->frame; m->b_cont != NULL; m = m->b_cont);
                    m->b_cont = om;
                }
                break;
            }
            case 28:
            {

                mblk_t *om = NULL;
                h264_packet_fu_a(nal, nal_len, &om);

                if (d->frame == NULL)
                {
                    d->frame = om;
                }
                else
                {
                    mblk_t *m = NULL;
                    for (m = d->frame; m->b_cont != NULL; m = m->b_cont);
                    m->b_cont = om;
                }
                break;
            }
            default:
            {
                printf("%s : Undefined type [%d]\n", __func__, type);
            }
        }

        if (marker)
        {
            if (d->frame != NULL)
            {
                msgpullup(d->frame, -1);

                timestamp = mblk_get_timestamp_info(im);
                timestamp = timestamp * 9 / 100;
//                printf("%s : timestamp = [%d]\n", __func__, timestamp);
                mblk_set_timestamp_info(d->frame, timestamp);

                ms_queue_put(f->outputs[0], d->frame);
                d->frame = NULL;
            }
        }

        freemsg(im);
    }
}

void h264_regroup_postprocess(struct _MSFilter *f)
{
    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);

}

void h264_regroup_uninit(struct _MSFilter *f)
{
    H264Regroup_t *d = NULL;

    printf("%s : %s : %d\n", __FILE__, __func__, __LINE__);
    if (f == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }
    d = (H264Regroup_t *)f->data;
    if (d == NULL)
    {
        printf("%s failed.\n", __func__);
        return;
    }

    if (d->frame) freemsg(d->frame);
    ms_free(d);
}


MSFilterDesc ms_h264_regroup_desc = {
    .id = MS_H264_REGROUP_ID,
    .name = "H264Regroup",
    .text = "regroup h264 frame",
    .category = MS_FILTER_OTHER,
    .enc_fmt = NULL,
    .ninputs = 1,
    .noutputs = 1,
    .init = h264_regroup_init,
    .preprocess = h264_regroup_preprocess,
    .process = h264_regroup_process,
    .postprocess = h264_regroup_postprocess,
    .uninit = h264_regroup_uninit,
    .methods = NULL,
};





