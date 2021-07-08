#ifndef __MSFILTER_H__
#define __MSFILTER_H__
#include <base/allfilter.h>
#include <base/msqueue.h>
#include <bctoolbox/bctoolbox.h>
//#include <msticker.h>



#define MS_SET_FILE_NAME            0
#define MS_SET_SRC_ADDR             1
#define MS_SET_DEST_ADDR            2
#define MS_PROBE_INPUT_FORMAT       3
#define MS_GET_SAMPLE_RATE          4
#define MS_GET_CHANNELS             5
#define MS_GET_SAMPLE_FMT           6
#define MS_GET_FRAME_SIZE           7
#define MS_SET_SAMPLE_RATE          8
#define MS_SET_CHANNELS             9
#define MS_SET_SAMPLE_FMT           10
#define MS_SET_FRAME_SIZE           11
#define MS_SET_MIME_TYPE            12
#define MS_SET_OUTPUT_SAMPLE_RATE   13
#define MS_SET_OUTPUT_CHANNELS      14
#define MS_SET_OUTPUT_SAMPLE_FMT    15
#define MS_SET_WIDTH                16
#define MS_SET_HEIGTH               17
#define MS_SET_PIX_FMT              18
#define MS_SET_OUTPUT_WIDTH         19
#define MS_SET_OUTPUT_HEIGTH        20
#define MS_SET_OUTPUT_PIX_FMT       21
#define MS_GET_WIDTH                22
#define MS_GET_HEIGTH               23
#define MS_GET_PIX_FMT              24
#define MS_SET_AMIX_INFO            25
#define MS_SET_VMIX_INFO            26


struct _MSFilter;


/**
 * Structure for filter's methods (init, preprocess, process, postprocess, uninit).
 * @var MSFilterFunc
 */
typedef void (*MSFilterFunc)(struct _MSFilter *f);

/**
 * Structure for filter's methods used to set filter's options.
 * @var MSFilterMethodFunc
 */
typedef int (*MSFilterMethodFunc)(struct _MSFilter *f, void *arg);

/**
 * Structure for filter's methods used as a callback to notify events.
 * @var MSFilterNotifyFunc
 */
typedef void (*MSFilterNotifyFunc)(void *userdata, struct _MSFilter *f, unsigned int id, void *arg);



typedef struct _MSFilterMethod
{
    unsigned int id;
    MSFilterMethodFunc method;
}MSFilterMethod;


typedef enum _MSFilterCategory{
    /**others*/
    MS_FILTER_OTHER,
    /**used by encoders*/
    MS_FILTER_ENCODER,
    /**used by decoders*/
    MS_FILTER_DECODER,
    /**used by capture filters that perform encoding*/
    MS_FILTER_ENCODING_CAPTURER,
    /**used by filters that perform decoding and rendering */
    MS_FILTER_DECODER_RENDERER
}MSFilterCategory;

/**
 * Filter's flags controlling special behaviours.
**/
typedef enum _MSFilterFlags{
	MS_FILTER_IS_PUMP = 1, /**< The filter must be called in process function every tick.*/
	/*...*/
	/*private flags: don't use it in filters.*/
	MS_FILTER_IS_ENABLED = 1<<31 /*<Flag to specify if a filter is enabled or not. Only enabled filters are returned by function ms_filter_get_encoder */
}MSFilterFlags;



typedef struct _MSFilterDesc
{
    MSFilterId id;              /**< the id declared in allfilters.h */
    const char *name;           /**< the filter name*/
    const char *text;           /**< short text describing the filter's function*/
    MSFilterCategory category;  /**< filter's category*/
    const char *enc_fmt;        /**< sub-mime of the format, must be set if category is MS_FILTER_ENCODER or MS_FILTER_DECODER */
    int ninputs;                /**< number of inputs */
    int noutputs;               /**< number of outputs */
    MSFilterFunc init;          /**< Filter's init function*/
    MSFilterFunc preprocess;    /**< Filter's preprocess function, called one time before starting to process*/
    MSFilterFunc process;       /**< Filter's process function, called every tick by the MSTicker to do the filter's job*/
    MSFilterFunc postprocess;   /**< Filter's postprocess function, called once after processing (the filter is no longer called in process() after)*/
    MSFilterFunc uninit;        /**< Filter's uninit function, used to deallocate internal structures*/
    MSFilterMethod *methods;    /**<Filter's method table*/
    unsigned int flags;         /**<Filter's special flags, from the MSFilterFlags enum.*/
}MSFilterDesc;



typedef struct _MSFilter
{
    MSFilterDesc *desc; /**<Back pointer to filter's descriptor.*/
    /*protected attributes, do not move or suppress any of them otherwise plugins will be broken */
//    ms_mutex_t lock;
    MSQueue **inputs; /**<Table of input queues.*/
    MSQueue **outputs;/**<Table of output queues */
//    struct _MSFactory *factory;/**<the factory that created this filter*/
//    void *padding; /**Unused - to be reused later when new protected fields have to added*/
    void *data; /**< Pointer used by the filter for internal state and computations.*/
    struct _MSTicker *ticker; /**<Pointer to the ticker object. It is never NULL when being called process()*/
    /*private attributes, they can be moved and changed at any time*/
    bctbx_list_t *notify_callbacks;
    uint32_t last_tick;
//    MSFilterStats *stats;
//    int postponed_task; /*number of postponed tasks*/
//    bool_t seen;
}MSFilter;

typedef struct _MSConnectionPoint
{
    MSFilter *filter; /**<Pointer to filter*/
    int pin; /**<Pin index on the filter*/
}MSConnectionPoint;


typedef struct _MSConnectionHelper
{
    MSConnectionPoint last;
}MSConnectionHelper;



void ms_filter_destroy(MSFilter *f);



void ms_connection_helper_start(MSConnectionHelper *h);
int ms_connection_helper_link(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin);
int ms_connection_helper_unlink(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin);




int ms_filter_link(MSFilter *f1, int pin1, MSFilter *f2, int pin2);
int ms_filter_unlink(MSFilter *f1, int pin1, MSFilter *f2, int pin2);


int ms_filter_call_method(MSFilter *f, unsigned int id, void *arg);

void ms_filter_add_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud, bool_t synchronous);
void ms_filter_set_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud);
void ms_filter_remove_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *ud);
void ms_filter_clear_notify_callback(MSFilter *f);
void ms_filter_notify(MSFilter *f, unsigned int id, void *arg);
void ms_filter_notify_no_arg(MSFilter *f, unsigned int id);




#endif


