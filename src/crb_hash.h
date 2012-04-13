#ifndef __CRB_HASH_H
#define __CRB_HASH_H 1

typedef struct crb_hash_item_s crb_hash_item_t;
struct crb_hash_item_s {
	crb_hash_item_t *next;
	void *ptr;
};

typedef struct crb_hash_s crb_hash_t;
struct crb_hash_s {
	crb_hash_item_t *items;
	ssize_t scale;
};

crb_hash_t *crb_hash_init(ssize_t scale);


#endif /* __CRB_HASH_H */
