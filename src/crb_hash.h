#ifndef __CRB_HASH_H
#define __CRB_HASH_H 1

#include <stdint.h>


typedef struct crb_hash_item_s crb_hash_item_t;
struct crb_hash_item_s {
	crb_hash_item_t *next;
	uint32_t key;
	void *data;
};

typedef struct crb_hash_s crb_hash_t;
struct crb_hash_s {
	crb_hash_item_t **items;
	ssize_t scale;
};

typedef struct crb_hash_cursor_s crb_hash_cursor_t;
struct crb_hash_cursor_s {
	crb_hash_t *hash;
	
	int col;
	crb_hash_item_t *row;
};

crb_hash_t *crb_hash_init(ssize_t scale);
void crb_hash_free(crb_hash_t *hash);
void *crb_hash_insert(crb_hash_t *hash, void *data, void *key, int key_len);
void *crb_hash_remove(crb_hash_t *hash, void *key, int key_len);
void *crb_hash_exists_key(crb_hash_t *hash, void *key, int key_len);

crb_hash_cursor_t *crb_hash_cursor_init(crb_hash_t *hash);
void *crb_hash_cursor_next(crb_hash_cursor_t *cursor);
void crb_hash_cursor_free(crb_hash_cursor_t *cursor);

uint32_t crb_murmurhash3(void * key, int len);


#endif /* __CRB_HASH_H */
