#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crb_hash.h"
#include "crb_atomic.h"

void crb_hash_item_ref(crb_hash_item_t *item);
void crb_hash_item_unref(crb_hash_item_t *item);
void crb_hash_item_free(crb_hash_item_t *item);

crb_hash_item_t *
crb_hash_item_init()
{
    crb_hash_item_t *item;

    item = malloc(sizeof(crb_hash_item_t));
    if (item == NULL) {
        return NULL;
    }
    
    item->key = 0;
    item->data = NULL;
    item->next = NULL;
    
    item->ref = 1;

    return item;
}

crb_hash_t *
crb_hash_init(ssize_t scale)
{
    crb_hash_t *hash;
    crb_hash_item_t *tmp;
    int i;

    hash = malloc(sizeof(crb_hash_t));
    if (hash == NULL) {
        return NULL;
    }
    
    hash->scale = scale;
    
    hash->items = calloc(scale, sizeof(crb_hash_item_t*));
        
    if ( hash->items == NULL ) {
    	free(hash);
    	return NULL;
    }
    
	hash->item_add = NULL;
	hash->item_remove = crb_hash_item_unref;

    return hash;
}

void 
crb_hash_free(crb_hash_t *hash)
{
	int col;
	crb_hash_item_t *tmp_item;
	crb_hash_item_t *new_item;
	
	// crb_hash_item_t **items;
	
	for (col = 0; col < hash->scale; col += 1) {
		tmp_item = hash->items[col];
		while ( tmp_item != NULL ) {
			new_item = tmp_item->next;
			crb_hash_item_unref(tmp_item);
			tmp_item = new_item;
		}
	}
	
	free(hash->items);
	free(hash);
}

void *
crb_hash_insert(crb_hash_t *hash, void *data, void *key, int key_len)
{
	uint32_t hash_key;
	crb_hash_item_t *new_item;
	crb_hash_item_t *tmp_item;
	int col;
	
	hash_key = crb_murmurhash3(key, key_len);
	col = hash_key%hash->scale;
	
	new_item = crb_hash_item_init();
	new_item->key = hash_key;
	new_item->data = data;
	
begin_insert:
	tmp_item = hash->items[col];
	if ( tmp_item == NULL ) {
		// first item is free
		new_item->next = NULL;
		
		if ( !crb_atomic_cmp_set(&(hash->items[col]), NULL, new_item) ) {
			goto begin_insert;
		}
	} else if( tmp_item->key > hash_key ) {
		// insert before first item
		new_item->next = tmp_item;
		
		if ( !crb_atomic_cmp_set(&(hash->items[col]), tmp_item, new_item) ) {
			goto begin_insert;
		}
	} else {
		do {
			tmp_item = hash->items[col];
			do {
				if ( tmp_item->key == hash_key ) {
					// duplicate
					crb_hash_item_free(new_item);
					return tmp_item->data;
				} else if( tmp_item->next != NULL && tmp_item->next->key > hash_key ) {
					// insert after current item
					break;
				}
			} while ( tmp_item->next != NULL && (tmp_item = tmp_item->next) );
	
			new_item->next = tmp_item->next;
		} while( !crb_atomic_cmp_set(&(tmp_item->next), new_item->next, new_item) );
	}
	
	if ( hash->item_add != NULL ) {
		hash->item_add(new_item);
	}
	
	return data;
}

void *
crb_hash_remove(crb_hash_t *hash, void *key, int key_len)
{
	uint32_t hash_key;
	crb_hash_item_t *found_item;
	crb_hash_item_t *tmp_item;
	void *data = NULL;
	
	hash_key = crb_murmurhash3(key, key_len);
	
	tmp_item = hash->items[hash_key%hash->scale];
	if ( tmp_item == NULL || tmp_item->key > hash_key ) {
		// missing scale
	} else if( tmp_item->key == hash_key ) {
		hash->items[hash_key%hash->scale] = tmp_item->next;
		
		data = tmp_item->data;
		
		if ( hash->item_remove != NULL ) {
			hash->item_remove(tmp_item);
		} else {
			crb_hash_item_unref(tmp_item);
		}
	} else {
		while ( tmp_item != NULL ) {
			if ( tmp_item->next != NULL && tmp_item->next->key == hash_key ) {
				// found
				found_item = tmp_item->next;
				crb_atomic_cmp_set(&(tmp_item->next), found_item, found_item->next);
				data = found_item->data;
				if ( hash->item_remove != NULL ) {
					hash->item_remove(found_item);
				} else {
					crb_hash_item_unref(found_item);
				}
				break;
			} else if( tmp_item->next != NULL && tmp_item->next->key < hash_key ) {
				tmp_item = tmp_item->next;
			} else {
				// not found
				break;
			}
		}
	}
	
	return data;
}

void *
crb_hash_exists_key(crb_hash_t *hash, void *key, int key_len)
{
	uint32_t hash_key;
	crb_hash_item_t *new_item;
	crb_hash_item_t *tmp_item;
	
	hash_key = crb_murmurhash3(key, key_len);
	
	tmp_item = hash->items[hash_key%hash->scale];
	if ( tmp_item == NULL || tmp_item->key > hash_key ) {
		return NULL;
	}
	
	do {
		if ( tmp_item->key == hash_key ) {
			// found
			return tmp_item->data;
		} else if( tmp_item->next != NULL && tmp_item->next->key > hash_key ) {
			// missing
			break;
		}
	} while ( tmp_item->next != NULL && (tmp_item = tmp_item->next) );

	return NULL;
}


crb_hash_cursor_t *
crb_hash_cursor_init(crb_hash_t *hash)
{
	crb_hash_cursor_t *cursor = malloc(sizeof(crb_hash_cursor_t));
	if ( cursor == NULL ) {
		return NULL;
	}
	
	cursor->hash = hash;
	cursor->col = 0;
	
	cursor->item_add = hash->item_add;
	cursor->item_remove = hash->item_remove;
	
	cursor->row = hash->items[0];
	if ( cursor->row != NULL ) {
		crb_hash_item_ref(cursor->row);
	}
	
	return cursor;
}

void *
crb_hash_cursor_next(crb_hash_cursor_t *cursor)
{
	void *data;
	crb_hash_item_t *tmp_item;
	
	if ( cursor->row != NULL ) {
		data = cursor->row->data;
		if ( cursor->row->next != NULL ) {
			crb_hash_item_ref(cursor->row->next);
		}
		tmp_item = cursor->row->next;
		crb_hash_item_unref(cursor->row);
		cursor->row = tmp_item;
		return data;
	} 
	
	while( cursor->col < cursor->hash->scale-1 ) {
		cursor->col++;
		cursor->row = cursor->hash->items[cursor->col];
		if ( cursor->row != NULL ) {
			if ( cursor->row->next != NULL ) {
				crb_hash_item_ref(cursor->row->next);
			}
			tmp_item = cursor->row->next;
			data = cursor->row->data;
			//crb_hash_item_unref(cursor->row);
			cursor->row = tmp_item;
			return data;
		}
	}
	
	return NULL;
}

void
crb_hash_cursor_free(crb_hash_cursor_t *cursor)
{
	cursor->hash = NULL;
	cursor->col = 0;
	
	if ( cursor->row != NULL ) {
		crb_hash_item_unref(cursor->row);
		cursor->row = NULL;
	}
	
	free(cursor);
}

inline void
crb_hash_item_ref(crb_hash_item_t *item)
{
	crb_atomic_fetch_add( &(item->ref), 1 );
}

inline void
crb_hash_item_unref(crb_hash_item_t *item)
{
	int old_ref;
	old_ref = __sync_sub_and_fetch( &(item->ref), 1 );
	if ( old_ref == 0 ) {
		crb_hash_item_free(item);
	}
}

void
crb_hash_item_free(crb_hash_item_t *item)
{
	item->next = item->data = NULL;
	item->key = 0;
	free(item);
}


inline uint32_t 
rotl32 (uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}

/*
 * MurmurHash3 by Austin Appleby
 */
uint32_t 
crb_murmurhash3(void * key, int len)
{
	const uint8_t * data = (const uint8_t*)key;
	const int nblocks = len / 4;

	uint32_t h1 = 0;

	int i;

	// body

	const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

	for(i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= 0xcc9e2d51;
		k1 = rotl32(k1,15);
		k1 *= 0x1b873593;

		h1 ^= k1;
		h1 = rotl32(h1,13); 
		h1 = h1*5+0xe6546b64;
	}

	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

	uint32_t k1 = 0;

	switch(len & 3) {
		case 3: 
			k1 ^= tail[2] << 16;
		case 2: 
			k1 ^= tail[1] << 8;
		case 1: 
			k1 ^= tail[0];
			k1 *= 0xcc9e2d51; 
			k1 = rotl32(k1,15); 
			k1 *= 0x1b873593; 
			h1 ^= k1;
	};

	// finalization

	h1 ^= len;

	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;

	return h1;
} 
