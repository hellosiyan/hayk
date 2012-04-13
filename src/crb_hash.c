#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crb_hash.h"

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

    return item;
}

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

void *
crb_hash_insert(crb_hash_t *hash, void *data, void *key, int key_len)
{
	uint32_t hash_key;
	crb_hash_item_t *new_item;
	crb_hash_item_t *tmp_item;
	
	hash_key = crb_murmurhash3(key, key_len);
	
	tmp_item = hash->items[hash_key%hash->scale];
	if ( tmp_item == NULL ) {
		// first item is free
		new_item = crb_hash_item_init();
		new_item->key = hash_key;
		new_item->data = data;
		new_item->next = NULL;
		hash->items[hash_key%hash->scale] = new_item;
	} else if( tmp_item->key > hash_key ) {
		// insert before first item
		new_item = crb_hash_item_init();
		new_item->key = hash_key;
		new_item->data = data;
		new_item->next = tmp_item;
		hash->items[hash_key%hash->scale] = new_item;
	} else {
		do {
			if ( tmp_item->key == hash_key ) {
				// duplicate
				return tmp_item->data;
			} else if( tmp_item->next != NULL && tmp_item->next->key > hash_key ) {
				// insert after current item
				break;
			}
		} while ( tmp_item->next != NULL && (tmp_item = tmp_item->next) );
	
		new_item = crb_hash_item_init();
		new_item->key = hash_key;
		new_item->data = data;
		new_item->next = tmp_item->next;
		tmp_item->next = new_item;
	}
	
	return data;
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
	cursor->row = hash->items[0];
	
	return cursor;
}

void *
crb_hash_cursor_next(crb_hash_cursor_t *cursor)
{
	if ( cursor->row != NULL && cursor->row->next != NULL ) {
		cursor->row = cursor->row->next;
		return cursor->row->data;
	}
	
	while( cursor->col < cursor->hash->scale ) {
		cursor->col++;
		cursor->row = cursor->hash->items[cursor->col];
		if ( cursor->row != NULL ) {
			return cursor->row->data;
		}
	}
	
	return NULL;
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
