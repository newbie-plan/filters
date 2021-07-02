#ifndef __BCTOOLBOX_H__
#define __BCTOOLBOX_H__

typedef void (*bctbx_list_free_func)(void *);
typedef struct _bctbx_list
{
    struct _bctbx_list *next;
    struct _bctbx_list *prev;
    void *data;
} bctbx_list_t;


bctbx_list_t * bctbx_list_new(void *data);
bctbx_list_t * bctbx_list_append_link(bctbx_list_t * elem, bctbx_list_t *new_elem);
bctbx_list_t * bctbx_list_append(bctbx_list_t * elem, void * data);
bctbx_list_t * bctbx_list_prepend_link(bctbx_list_t* elem, bctbx_list_t *new_elem);
bctbx_list_t * bctbx_list_prepend(bctbx_list_t * elem, void * data);
bctbx_list_t* bctbx_list_next(const bctbx_list_t *elem);
bctbx_list_t*  bctbx_list_concat(bctbx_list_t* first, bctbx_list_t* second);
bctbx_list_t* bctbx_list_unlink(bctbx_list_t* list, bctbx_list_t* elem);
bctbx_list_t * bctbx_list_erase_link(bctbx_list_t* list, bctbx_list_t* elem);
bctbx_list_t * bctbx_list_free(bctbx_list_t * elem);
bctbx_list_t * bctbx_list_free_with_data(bctbx_list_t *list, bctbx_list_free_func freefunc);


#endif

