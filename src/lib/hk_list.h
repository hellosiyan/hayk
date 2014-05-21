#ifndef __HK_LIST_H
#define __HK_LIST_H 1

#include <stdint.h>

typedef struct hk_list_item_s hk_list_item_t;
struct hk_list_item_s {
	uint32_t ref;
	hk_list_item_t *prev;
	hk_list_item_t *next;
	void *data;
};

typedef struct hk_list_s hk_list_t;
struct hk_list_s {
	hk_list_item_t *first;
	hk_list_item_t *last;
	uint32_t length;
};

hk_list_t *hk_list_init();
void hk_list_free(hk_list_t *list);

void hk_list_push(hk_list_t *list, void *data);
void hk_list_unshift(hk_list_t *list, void *data);
void *hk_list_pop(hk_list_t *list);

void hk_list_item_ref(hk_list_item_t *item);
void hk_list_item_unref(hk_list_item_t *item);


#endif /* __HK_LIST_H */
