#ifndef __CRB_HASH_H
#define __CRB_HASH_H 1

#include <stdint.h>


typedef struct crb_hash_item_s crb_hash_item_t;
struct crb_hash_item_s {
	crb_hash_item_t *next;
	uint32_t key;
	void *data;
	
	int32_t ref;
};

typedef struct crb_hash_s crb_hash_t;
struct crb_hash_s {
	crb_hash_item_t **items;
	size_t scale;
	
	void (*item_add)(crb_hash_item_t *item);
	void (*item_remove)(crb_hash_item_t *item);
};

typedef struct crb_hash_cursor_s crb_hash_cursor_t;
struct crb_hash_cursor_s {
	crb_hash_t *hash;
	
	void (*item_add)(crb_hash_item_t *item);
	void (*item_remove)(crb_hash_item_t *item);
	
	uint8_t col;
	crb_hash_item_t *row;
};

crb_hash_t *crb_hash_init(size_t scale);
void crb_hash_free(crb_hash_t *hash);
void *crb_hash_insert(crb_hash_t *hash, void *data, void *key, int key_len);
void *crb_hash_remove(crb_hash_t *hash, void *key, int key_len);
void *crb_hash_exists_key(crb_hash_t *hash, void *key, int key_len);

crb_hash_cursor_t *crb_hash_cursor_init(crb_hash_t *hash);
void *crb_hash_cursor_next(crb_hash_cursor_t *cursor);
void crb_hash_cursor_free(crb_hash_cursor_t *cursor);

uint32_t crb_murmurhash3(void * key, int len);


#endif /* __CRB_HASH_H */
