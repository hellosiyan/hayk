#ifndef __CRB_LIST_H
#define __CRB_LIST_H 1

typedef struct crb_list_item_s crb_list_item_t;
struct crb_list_item_s {
	crb_list_item_t *next;
	void *ptr;
};

typedef struct crb_list_s crb_list_t;
struct crb_list_s {
	crb_list_item_t *items;
	ssize_t scale;
};

crb_list_t *crb_list_init(ssize_t scale);


#endif /* __CRB_LIST_H */
