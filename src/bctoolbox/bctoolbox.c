#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <base/mscommon.h>
#include <bctoolbox/bctoolbox.h>


bctbx_list_t* bctbx_list_new(void *data)
{
    bctbx_list_t* new_elem = ms_new0(bctbx_list_t,1);
    new_elem->data = data;
    return new_elem;
}


bctbx_list_t*  bctbx_list_append_link(bctbx_list_t* elem,bctbx_list_t *new_elem)
{
    bctbx_list_t* it=elem;
    if (elem==NULL)  return new_elem;
    if (new_elem==NULL)  return elem;
    while (it->next != NULL) it = bctbx_list_next(it);
    it->next = new_elem;
    new_elem->prev = it;
    return elem;
}


bctbx_list_t*  bctbx_list_append(bctbx_list_t* elem, void * data)
{
    bctbx_list_t* new_elem = bctbx_list_new(data);
    return bctbx_list_append_link(elem, new_elem);
}


bctbx_list_t*  bctbx_list_prepend_link(bctbx_list_t* elem, bctbx_list_t *new_elem)
{
    if (elem != NULL) {
        new_elem->next = elem;
        elem->prev = new_elem;
    }
    return new_elem;
}

bctbx_list_t*  bctbx_list_prepend(bctbx_list_t* elem, void *data)
{
    return bctbx_list_prepend_link(elem,bctbx_list_new(data));
}


bctbx_list_t* bctbx_list_next(const bctbx_list_t *elem)
{
    return elem->next;
}


bctbx_list_t*  bctbx_list_concat(bctbx_list_t* first, bctbx_list_t* second)
{
    bctbx_list_t* it = first;
    if (it == NULL) return second;
    if (second == NULL) return first;
    while(it->next != NULL) it = bctbx_list_next(it);
    it->next = second;
    second->prev = it;
    return first;
}

bctbx_list_t* bctbx_list_unlink(bctbx_list_t* list, bctbx_list_t* elem)
{
    bctbx_list_t* ret;
    if (elem == list)
    {
        ret = elem->next;
        elem->prev = NULL;
        elem->next = NULL;
        if (ret != NULL) ret->prev = NULL;
        return ret;
    }
    elem->prev->next = elem->next;
    if (elem->next != NULL) elem->next->prev = elem->prev;
    elem->next = NULL;
    elem->prev = NULL;
    return list;
}


bctbx_list_t * bctbx_list_erase_link(bctbx_list_t* list, bctbx_list_t* elem)
{
    bctbx_list_t *ret = bctbx_list_unlink(list,elem);
    ms_free(elem);
    return ret;
}


bctbx_list_t*  bctbx_list_free(bctbx_list_t* list)
{
    bctbx_list_t* elem = list;
    bctbx_list_t* tmp;
    if (list == NULL) return NULL;
    while(elem->next != NULL)
    {
        tmp = elem;
        elem = elem->next;
        ms_free(tmp);
    }
    ms_free(elem);
    return NULL;
}

bctbx_list_t * bctbx_list_free_with_data(bctbx_list_t *list, bctbx_list_free_func freefunc)
{
    bctbx_list_t* elem = list;
    bctbx_list_t* tmp;
    if (list == NULL) return NULL;
    while(elem->next != NULL)
    {
        tmp = elem;
        elem = elem->next;
        freefunc(tmp->data);
        ms_free(tmp);
    }
    freefunc(elem->data);
    ms_free(elem);
    return NULL;
}


