#include <base/msfactory.h>
#include <base/mscommon.h>
#include <base/alldescs.h>


MSFactory *ms_factory_new(void)
{
    MSFactory *factory=ms_new0(MSFactory,1);
    ms_factory_init(factory);
    return factory;
}

void ms_factory_init(MSFactory *factory)
{
    int i = 0;
    if (factory == NULL)
    {
        printf("ms_factory_init failed.\n");
        return;
    }

    for (i = 0; ms_filter_descs[i] != NULL; i++)
    {
        ms_factory_register_filter(factory, ms_filter_descs[i]);
    }
}

void ms_factory_destroy(MSFactory *factory)
{
    if (factory == NULL)
    {
        printf("ms_factory_destroy failed.\n");
        return;
    }

    factory->desc_list = bctbx_list_free(factory->desc_list);
    ms_free(factory);
}



void ms_factory_register_filter(MSFactory* factory, MSFilterDesc* desc)
{
    if (factory == NULL || desc == NULL)
    {
        printf("ms_factory_register_filter failed.\n");
        return;
    }

    /*lastly registered encoder/decoders may replace older ones*/
    factory->desc_list = bctbx_list_prepend(factory->desc_list,desc);
}



MSFilter *ms_factory_create_filter(MSFactory* factory, MSFilterId id)
{
    MSFilterDesc *desc;
    /*if (id==MS_FILTER_PLUGIN_ID)
    {
        printf("cannot create plugin filters with ms_filter_new_from_id()");
        return NULL;
    }*/
    desc = ms_factory_lookup_filter_by_id(factory,id);
    if (desc) return ms_factory_create_filter_from_desc(factory,desc);

    printf("No such filter with id [%i].\n",id);
    return NULL;
}
MSFilter *ms_factory_create_filter_from_desc(MSFactory* factory, MSFilterDesc *desc)
{
    MSFilter *obj;
    obj = (MSFilter *)ms_new0(MSFilter,1);
//    ms_mutex_init(&obj->lock,NULL);
    obj->desc = desc;
    if (desc->ninputs > 0)    obj->inputs = (MSQueue**)ms_new0(MSQueue*, desc->ninputs);
    if (desc->noutputs > 0)   obj->outputs = (MSQueue**)ms_new0(MSQueue*, desc->noutputs);

#if 0
    if (factory->statistics_enabled)
    {
        obj->stats=find_or_create_stats(factory,desc);
    }
    obj->factory=factory;
#endif

    if (obj->desc->init != NULL)  obj->desc->init(obj);
    return obj;
}

MSFilterDesc * ms_factory_get_encoder(MSFactory* factory, const char *mime){
	bctbx_list_t *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=bctbx_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if ((desc->flags & MS_FILTER_IS_ENABLED)
			&& (desc->category==MS_FILTER_ENCODER || desc->category==MS_FILTER_ENCODING_CAPTURER)
			&& strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilterDesc * ms_factory_get_decoder(MSFactory* factory, const char *mime){
	bctbx_list_t *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=bctbx_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if ((desc->flags & MS_FILTER_IS_ENABLED)
			&& (desc->category==MS_FILTER_DECODER || desc->category==MS_FILTER_DECODER_RENDERER)
			&& strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}


MSFilter * ms_factory_create_encoder(MSFactory* factory, const char *mime){
	MSFilterDesc *desc=ms_factory_get_encoder(factory,mime);
	if (desc!=NULL) return ms_factory_create_filter_from_desc(factory,desc);
	return NULL;
}

MSFilter * ms_factory_create_decoder(MSFactory* factory, const char *mime){
	//MSFilterDesc *desc=ms_filter_get_decoder(mime);
	MSFilterDesc *desc = ms_factory_get_decoder(factory, mime);
	if (desc!=NULL) return ms_factory_create_filter_from_desc(factory,desc);
	return NULL;
}



MSFilterDesc* ms_factory_lookup_filter_by_id( MSFactory* factory, MSFilterId id)
{
    bctbx_list_t *elem;

    for (elem = factory->desc_list; elem != NULL; elem = bctbx_list_next(elem))
    {
        MSFilterDesc *desc=(MSFilterDesc*)elem->data;
        if (desc->id == id)
        {
            return desc;
        }
    }
    return NULL;
}



