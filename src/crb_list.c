#include <stdlib.h>

#include "crb_list.h"

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
crb_list_push(crb_list_t *list, void *data)
{
	crb_list_item_t *item = malloc(sizeof(crb_list_item_t));
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

void
crb_list_remove(crb_list_t *list, void *data)
{
	// TODO
}
