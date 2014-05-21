#ifndef __HK_TASK_H__
#define __HK_TASK_H__ 1

#include "hk_buffer.h"
#include "hk_client.h"

typedef enum {
     HK_TASK_NULL = 0,
     HK_TASK_BROADCAST,
     HK_TASK_PONG,
     HK_TASK_HANDSHAKE,
     HK_TASK_SHUTDOWN
} hk_task_type_e;

typedef struct hk_task_s hk_task_t;
struct hk_task_s {
	hk_task_type_e type;
	
	hk_client_t *client;
	hk_buffer_t *buffer;
	
	void *data;
	void *data2;
	
	hk_task_t *prev; /* used in queues */
};

typedef struct hk_task_queue_s hk_task_queue_t;
struct hk_task_queue_s {
	hk_task_t *first;
	hk_task_t *last;
	uint32_t length;
};

hk_task_t *hk_task_init();
void hk_task_free(hk_task_t *task);
void hk_task_set_client(hk_task_t *task, hk_client_t *client);
void hk_task_set_buffer(hk_task_t *task, hk_buffer_t *buffer);
void hk_task_set_data(hk_task_t *task, void *data);
void hk_task_set_data2(hk_task_t *task, void *data);
void hk_task_set_type(hk_task_t *task, hk_task_type_e type);

hk_task_queue_t *hk_task_queue_init();
void hk_task_queue_free(hk_task_queue_t *queue);
void hk_task_queue_push(hk_task_queue_t *queue, hk_task_t *task);
hk_task_t * hk_task_queue_pop(hk_task_queue_t *queue);

#endif /* __HK_TASK_H__ */
