#include <stdlib.h>

#include "crb_list.h"

crb_list_t *
crb_list_init(ssize_t scale)
{
    crb_list_t *list;

    list = malloc(sizeof(crb_list_t));
    if (list == NULL) {
        return NULL;
    }
    
    list->scale = scale;
    
    list->items = calloc(scale, sizeof(crb_list_item_t));
    if ( list->items == NULL ) {
    	free(list);
    	return NULL;
    }

    return list;
}

