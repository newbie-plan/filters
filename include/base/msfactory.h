#ifndef __MS_FACTORY_H__
#define __MS_FACTORY_H__
#include <base/msfilter.h>
#include <base/allfilter.h>
#include <bctoolbox/bctoolbox.h>



/*do not use these fields directly*/
typedef struct _MSFactory{
    bctbx_list_t *desc_list;
#if 0
    MSList *stats_list;
    MSList *offer_answer_provider_list;
#ifdef _WIN32
    MSList *ms_plugins_loaded_list;
#endif
    MSList *formats;
    MSList *platform_tags;
    char *plugins_dir;
    struct _MSVideoPresetsManager *video_presets_manager;
    int cpu_count;
    struct _MSEventQueue *evq;
    int max_payload_size;
    int mtu;
    struct _MSSndCardManager* sndcardmanager;
    struct _MSWebCamManager* wbcmanager;
    void (*voip_uninit_func)(struct _MSFactory*);
    bool_t statistics_enabled;
    bool_t voip_initd;
    MSDevicesInfo *devices_info;
    char *image_resources_dir;
    char *echo_canceller_filtername;
    int expected_video_bandwidth;
#endif
}MSFactory;


MSFactory *ms_factory_new(void);
void ms_factory_init(MSFactory *obj);
void ms_factory_destroy(MSFactory *factory);
void ms_factory_register_filter(MSFactory* factory, MSFilterDesc* desc);
MSFilter *ms_factory_create_filter(MSFactory* factory, MSFilterId id);
MSFilter *ms_factory_create_filter_from_desc(MSFactory* factory, MSFilterDesc *desc);
MSFilterDesc* ms_factory_lookup_filter_by_id( MSFactory* factory, MSFilterId id);
MSFilter * ms_factory_create_encoder(MSFactory* factory, const char *mime);
MSFilter * ms_factory_create_decoder(MSFactory* factory, const char *mime);



#endif


