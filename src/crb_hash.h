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

crb_hash_t *crb_hash_init(ssize_t scale);
int crb_hash_insert(crb_hash_t *hash, void *data, void *key, int key_len);

uint32_t crb_murmurhash3(void * key, int len);


#endif /* __CRB_HASH_H */
