#include <stdlib.h>
#include <stdint.h>

#include "hk_list.h"
#include "hk_atomic.h"

hk_list_t *
hk_list_init()
{
    hk_list_t *list;

    list = malloc(sizeof(hk_list_t));
    if (list == NULL) {
        return NULL;
    }
    
    list->first = NULL;
    list->last = NULL;
    list->length = 0;

    return list;
}

void
hk_list_free(hk_list_t *list)
{
    while ( hk_list_pop(list) ) {
    	// pass
    }
    
    free(list);
}

void 
hk_list_push(hk_list_t *list, void *data)
{
	hk_list_item_t *item = malloc(sizeof(hk_list_item_t));
	item->ref = 1;
	item->data = data;
	item->next = NULL;
	item->prev = list->last;
	
	if ( list->last != NULL ) {
		list->last->next = item;
	}
	
	list->last = item;
	if ( list->first == NULL ) {
		list->first = item;
	}
}

void 
hk_list_unshift(hk_list_t *list, void *data)
{
	hk_list_item_t *item = malloc(sizeof(hk_list_item_t));
	item->ref = 1;
	item->data = data;
	item->next = list->first;
	item->prev = NULL;
	
	if ( list->first != NULL ) {
		list->first->prev = item;
	}
	
	list->first = item;
	if ( list->last == NULL ) {
		list->last = item;
	}
}

void *
hk_list_pop(hk_list_t *list)
{
	hk_list_item_t *tmp;
	void *data = NULL;
	
	if ( list->last == NULL ) {
		return NULL;
	}
	
	tmp = list->last;
	
	while( !hk_atomic_cmp_set(&(list->last), tmp, tmp->prev) ) {
		tmp = list->last;
	}
	
	data = tmp->data;
	
	hk_list_item_unref(tmp);
	
	return data;
}

void 
hk_list_item_ref(hk_list_item_t *item)
{
	hk_atomic_fetch_add( &(item->ref), 1 );
}

void 
hk_list_item_unref(hk_list_item_t *item) 
{
	uint32_t old_ref;
	old_ref = hk_atomic_sub_fetch( &(item->ref), 1 );
	if ( old_ref == 0 ) {
		free(item);
	}
}

