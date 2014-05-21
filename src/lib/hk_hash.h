#ifndef __HK_HASH_H
#define __HK_HASH_H 1

#include <stdint.h>


typedef struct hk_hash_item_s hk_hash_item_t;
struct hk_hash_item_s {
	hk_hash_item_t *next;
	uint32_t key;
	void *data;
	
	int32_t ref;
};

typedef struct hk_hash_s hk_hash_t;
struct hk_hash_s {
	hk_hash_item_t **items;
	size_t scale;
	
	void (*item_add)(hk_hash_item_t *item);
	void (*item_remove)(hk_hash_item_t *item);
};

typedef struct hk_hash_cursor_s hk_hash_cursor_t;
struct hk_hash_cursor_s {
	hk_hash_t *hash;
	
	void (*item_add)(hk_hash_item_t *item);
	void (*item_remove)(hk_hash_item_t *item);
	
	uint8_t col;
	hk_hash_item_t *row;
};

hk_hash_t *hk_hash_init(size_t scale);
void hk_hash_free(hk_hash_t *hash);
void *hk_hash_insert(hk_hash_t *hash, void *data, void *key, int key_len);
void *hk_hash_remove(hk_hash_t *hash, void *key, int key_len);
void *hk_hash_exists_key(hk_hash_t *hash, void *key, int key_len);

hk_hash_cursor_t *hk_hash_cursor_init(hk_hash_t *hash);
void *hk_hash_cursor_next(hk_hash_cursor_t *cursor);
void hk_hash_cursor_free(hk_hash_cursor_t *cursor);

uint32_t hk_murmurhash3(void * key, int len);


#endif /* __HK_HASH_H */
