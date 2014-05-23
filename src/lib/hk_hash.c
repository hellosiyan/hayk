#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hk_hash.h"
#include "hk_atomic.h"

void hk_hash_item_ref(hk_hash_item_t *item);
void hk_hash_item_unref(hk_hash_item_t *item);
void hk_hash_item_free(hk_hash_item_t *item);

hk_hash_item_t *
hk_hash_item_init()
{
    hk_hash_item_t *item;

    item = malloc(sizeof(hk_hash_item_t));
    if (item == NULL) {
        return NULL;
    }
    
    item->key = 0;
    item->data = NULL;
    item->next = NULL;
    
    item->ref = 1;

    return item;
}

hk_hash_t *
hk_hash_init(size_t scale)
{
    hk_hash_t *hash;
    hk_hash_item_t *tmp;
    int i;

    hash = malloc(sizeof(hk_hash_t));
    if (hash == NULL) {
        return NULL;
    }
    
    hash->scale = scale;
    
    hash->items = calloc(scale, sizeof(hk_hash_item_t*));
        
    if ( hash->items == NULL ) {
    	free(hash);
    	return NULL;
    }
    
	hash->item_add = NULL;
	hash->item_remove = hk_hash_item_unref;

    return hash;
}

void 
hk_hash_free(hk_hash_t *hash)
{
	int col;
	hk_hash_item_t *tmp_item;
	hk_hash_item_t *new_item;
	
	for (col = 0; col < hash->scale; col += 1) {
		tmp_item = hash->items[col];
		while ( tmp_item != NULL ) {
			new_item = tmp_item->next;
			hk_hash_item_unref(tmp_item);
			tmp_item = new_item;
		}
	}
	
	free(hash->items);
	free(hash);
}

void *
hk_hash_insert(hk_hash_t *hash, void *data, void *key, int key_len)
{
	uint32_t hash_key;
	hk_hash_item_t *new_item;
	hk_hash_item_t *tmp_item;
	int col;
	
	hash_key = hk_murmurhash3(key, key_len);
	col = hash_key%hash->scale;
	
	new_item = hk_hash_item_init();
	new_item->key = hash_key;
	new_item->data = data;
	
begin_insert:
	tmp_item = hash->items[col];
	if ( tmp_item == NULL ) {
		// first item is free
		new_item->next = NULL;
		
		if ( !hk_atomic_cmp_set(&(hash->items[col]), NULL, new_item) ) {
			goto begin_insert;
		}
	} else if( tmp_item->key > hash_key ) {
		// insert before first item
		new_item->next = tmp_item;
		
		if ( !hk_atomic_cmp_set(&(hash->items[col]), tmp_item, new_item) ) {
			goto begin_insert;
		}
	} else {
		do {
			tmp_item = hash->items[col];
			do {
				if ( tmp_item->key == hash_key ) {
					// duplicate
					hk_hash_item_free(new_item);
					return tmp_item->data;
				} else if( tmp_item->next != NULL && tmp_item->next->key > hash_key ) {
					// insert after current item
					break;
				}
			} while ( tmp_item->next != NULL && (tmp_item = tmp_item->next) );
	
			new_item->next = tmp_item->next;
		} while( !hk_atomic_cmp_set(&(tmp_item->next), new_item->next, new_item) );
	}
	
	if ( hash->item_add != NULL ) {
		hash->item_add(new_item);
	}
	
	return data;
}

void *
hk_hash_remove(hk_hash_t *hash, void *key, int key_len)
{
	uint32_t hash_key;
	hk_hash_item_t *found_item;
	hk_hash_item_t *tmp_item;
	void *data = NULL;
	
	hash_key = hk_murmurhash3(key, key_len);
	
	tmp_item = hash->items[hash_key%hash->scale];
	if ( tmp_item == NULL || tmp_item->key > hash_key ) {
		// missing scale
	} else if( tmp_item->key == hash_key ) {
		hash->items[hash_key%hash->scale] = tmp_item->next;
		
		data = tmp_item->data;
		
		if ( hash->item_remove != NULL ) {
			hash->item_remove(tmp_item);
		} else {
			hk_hash_item_unref(tmp_item);
		}
	} else {
		while ( tmp_item != NULL ) {
			if ( tmp_item->next != NULL && tmp_item->next->key == hash_key ) {
				// found
				found_item = tmp_item->next;
				hk_atomic_cmp_set(&(tmp_item->next), found_item, found_item->next);
				data = found_item->data;
				if ( hash->item_remove != NULL ) {
					hash->item_remove(found_item);
				} else {
					hk_hash_item_unref(found_item);
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
hk_hash_exists_key(hk_hash_t *hash, void *key, int key_len)
{
	uint32_t hash_key;
	hk_hash_item_t *new_item;
	hk_hash_item_t *tmp_item;
	
	hash_key = hk_murmurhash3(key, key_len);
	
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


hk_hash_cursor_t *
hk_hash_cursor_init(hk_hash_t *hash)
{
	hk_hash_cursor_t *cursor = malloc(sizeof(hk_hash_cursor_t));
	if ( cursor == NULL ) {
		return NULL;
	}
	
	cursor->hash = hash;
	cursor->col = 0;
	
	cursor->item_add = hash->item_add;
	cursor->item_remove = hash->item_remove;
	
	cursor->row = hash->items[0];
	if ( cursor->row != NULL ) {
		hk_hash_item_ref(cursor->row);
	}
	
	return cursor;
}

void *
hk_hash_cursor_next(hk_hash_cursor_t *cursor)
{
	void *data;
	hk_hash_item_t *tmp_item;
	
	if ( cursor->row != NULL ) {
		data = cursor->row->data;
		if ( cursor->row->next != NULL ) {
			hk_hash_item_ref(cursor->row->next);
		}
		tmp_item = cursor->row->next;
		hk_hash_item_unref(cursor->row);
		cursor->row = tmp_item;
		return data;
	} 
	
	while( cursor->col < cursor->hash->scale-1 ) {
		cursor->col++;
		cursor->row = cursor->hash->items[cursor->col];
		if ( cursor->row != NULL ) {
			if ( cursor->row->next != NULL ) {
				hk_hash_item_ref(cursor->row->next);
			}
			tmp_item = cursor->row->next;
			data = cursor->row->data;
			cursor->row = tmp_item;
			return data;
		}
	}
	
	return NULL;
}

void
hk_hash_cursor_free(hk_hash_cursor_t *cursor)
{
	cursor->hash = NULL;
	cursor->col = 0;
	
	if ( cursor->row != NULL ) {
		hk_hash_item_unref(cursor->row);
		cursor->row = NULL;
	}
	
	free(cursor);
}

inline void
hk_hash_item_ref(hk_hash_item_t *item)
{
	hk_atomic_fetch_add( &(item->ref), 1 );
}

inline void
hk_hash_item_unref(hk_hash_item_t *item)
{
	uint32_t old_ref;
	old_ref = hk_atomic_sub_fetch( &(item->ref), 1 );
	if ( old_ref == 0 ) {
		hk_hash_item_free(item);
	}
}

void
hk_hash_item_free(hk_hash_item_t *item)
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
hk_murmurhash3(void * key, int len)
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
