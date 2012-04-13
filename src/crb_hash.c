#include <stdlib.h>

#include "crb_hash.h"

crb_hash_t *
crb_hash_init(ssize_t scale)
{
    crb_hash_t *hash;

    hash = malloc(sizeof(crb_hash_t));
    if (hash == NULL) {
        return NULL;
    }
    
    hash->scale = scale;
    
    hash->items = calloc(scale, sizeof(crb_hash_item_t));
    if ( hash->items == NULL ) {
    	free(hash);
    	return NULL;
    }

    return hash;
}

