#include <stdio.h>
#include <stdlib.h>

#include "hk_task.h"

hk_task_t *
hk_task_init()
{
    hk_task_t *task;

    task = malloc(sizeof(hk_task_t));
    if (task == NULL) {
        return NULL;
    }
    
    task->client = NULL;
    task->buffer = NULL;
    task->data = NULL;
    task->data2 = NULL;
    task->prev = NULL;
    
    task->type = CRB_TASK_NULL;

    return task;
}

void
hk_task_free(hk_task_t *task)
{	
	if ( task->client != NULL ) {
		hk_client_unref(task->client);
		task->client = NULL;
	}
	task->buffer = NULL;
	task->data = NULL;
	task->prev = NULL;
	
	free(task);
}

void 
hk_task_set_client(hk_task_t *task, hk_client_t *client)
{
	if ( task->client == client ) {
		return;
	} else if ( task->client != NULL ) {
		hk_client_unref(task->client);
	}
	
	hk_client_ref(client);
	task->client = client;
}

void 
hk_task_set_buffer(hk_task_t *task, hk_buffer_t *buffer)
{
	task->buffer = buffer;
}

void 
hk_task_set_data(hk_task_t *task, void *data)
{
	task->data = data;
}

void 
hk_task_set_data2(hk_task_t *task, void *data)
{
	task->data2 = data;
}

void 
hk_task_set_type(hk_task_t *task, hk_task_type_e type)
{
	task->type = type;
}

hk_task_queue_t *
hk_task_queue_init()
{
    hk_task_queue_t *queue;

    queue = malloc(sizeof(hk_task_queue_t));
    if (queue == NULL) {
        return NULL;
    }
    
    queue->first = NULL;
    queue->last = NULL;
    queue->length = 0;

    return queue;
}

void
hk_task_queue_free(hk_task_queue_t *queue)
{	
	while( hk_task_queue_pop(queue) ) {
		// pass
	}
	
	free(queue);
}


void 
hk_task_queue_push(hk_task_queue_t *queue, hk_task_t *task)
{
	task->prev = NULL;
	
	if ( queue->last ) {
		queue->last->prev = task;
	}
	
	if ( queue->first == NULL ) {
		queue->first = task;
	}
	
	queue->last = task;
	queue->length ++;
}

hk_task_t *
hk_task_queue_pop(hk_task_queue_t *queue)
{
	hk_task_t *pop = NULL;
	
	if ( queue->first != NULL ) {
		pop = queue->first;
		if ( queue->first->prev == NULL ) {
			queue->first = queue->last = NULL;
		} else {
			queue->first = queue->first->prev;
		}
		pop->prev = NULL;
		
		queue->length --;
	}
	
	return pop;
}

