#ifndef __CRB_TASK_H__
#define __CRB_TASK_H__ 1

#include "crb_buffer.h"
#include "crb_client.h"

typedef enum {
     CRB_TASK_NULL = 0,
     CRB_TASK_BROADCAST,
     CRB_TASK_PONG,
     CRB_TASK_HANDSHAKE,
     CRB_TASK_SHUTDOWN
} crb_task_type_e;

typedef struct crb_task_s crb_task_t;
struct crb_task_s {
	crb_task_type_e type;
	
	crb_client_t *client;
	crb_buffer_t *buffer;
	
	void *data;
	void *data2;
	
	crb_task_t *prev; /* used in queues */
};

typedef struct crb_task_queue_s crb_task_queue_t;
struct crb_task_queue_s {
	crb_task_t *first;
	crb_task_t *last;
	uint32_t length;
};

crb_task_t *crb_task_init();
void crb_task_free(crb_task_t *task);
void crb_task_set_client(crb_task_t *task, crb_client_t *client);
void crb_task_set_buffer(crb_task_t *task, crb_buffer_t *buffer);
void crb_task_set_data(crb_task_t *task, void *data);
void crb_task_set_data2(crb_task_t *task, void *data);
void crb_task_set_type(crb_task_t *task, crb_task_type_e type);

crb_task_queue_t *crb_task_queue_init();
void crb_task_queue_free(crb_task_queue_t *queue);
void crb_task_queue_push(crb_task_queue_t *queue, crb_task_t *task);
crb_task_t * crb_task_queue_pop(crb_task_queue_t *queue);

#endif /* __CRB_TASK_H__ */
