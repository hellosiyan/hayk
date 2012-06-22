#include <stdlib.h>

#include "crb_list.h"
#include "crb_atomic.h"

crb_list_t *
crb_list_init()
{
    crb_list_t *list;

    list = malloc(sizeof(crb_list_t));
    if (list == NULL) {
        return NULL;
    }
    
    list->first = NULL;
    list->last = NULL;
    list->length = 0;

    return list;
}

void
crb_list_free(crb_list_t *list)
{
    while ( crb_list_pop(list) ) {
    	// pass
    }
    
    free(list);
}

void 
crb_list_push(crb_list_t *list, void *data)
{
	crb_list_item_t *item = malloc(sizeof(crb_list_item_t));
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
crb_list_unshift(crb_list_t *list, void *data)
{
	crb_list_item_t *item = malloc(sizeof(crb_list_item_t));
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
crb_list_pop(crb_list_t *list)
{
	crb_list_item_t *tmp;
	void *data = NULL;
	
	if ( list->last == NULL ) {
		return NULL;
	}
	
	tmp = list->last;
	
	while( !crb_atomic_cmp_set(&(list->last), tmp, tmp->prev) ) {
		tmp = list->last;
	}
	
	data = tmp->data;
	
	crb_list_item_unref(tmp);
	
	return data;
}

void
crb_list_remove(crb_list_t *list, void *data)
{
	// TODO
}

void 
crb_list_item_ref(crb_list_item_t *item)
{
	crb_atomic_fetch_add( &(item->ref), 1 );
}

void 
crb_list_item_unref(crb_list_item_t *item) 
{
	int old_ref;
	old_ref = __sync_sub_and_fetch( &(item->ref), 1 );
	if ( old_ref == 0 ) {
		free(item);
	}
}

