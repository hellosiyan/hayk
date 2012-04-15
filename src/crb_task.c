#include <stdio.h>
#include <stdlib.h>

#include "crb_task.h"

crb_task_t *
crb_task_init()
{
    crb_task_t *task;

    task = malloc(sizeof(crb_task_t));
    if (task == NULL) {
        return NULL;
    }
    
    task->client = NULL;
    task->buffer = NULL;
    task->data = NULL;
    task->prev = NULL;
    
    task->type = CRB_TASK_NULL;

    return task;
}

void
crb_task_free(crb_task_t *task)
{	
	if ( task->client != NULL ) {
		crb_client_unref(task->client);
		task->client = NULL;
	}
	task->buffer = NULL;
	task->data = NULL;
	task->prev = NULL;
	
	free(task);
}

void 
crb_task_set_client(crb_task_t *task, crb_client_t *client)
{
	if ( task->client == client ) {
		return;
	} else if ( task->client != NULL ) {
		crb_client_unref(task->client);
	}
	
	task->client = client;
	crb_client_ref(client);
}

crb_task_queue_t *
crb_task_queue_init()
{
    crb_task_queue_t *queue;

    queue = malloc(sizeof(crb_task_queue_t));
    if (queue == NULL) {
        return NULL;
    }
    
    queue->first = NULL;
    queue->last = NULL;
    queue->length = 0;

    return queue;
}


void 
crb_task_queue_push(crb_task_queue_t *queue, crb_task_t *task)
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

crb_task_t *
crb_task_queue_pop(crb_task_queue_t *queue)
{
	crb_task_t *pop = NULL;
	
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

