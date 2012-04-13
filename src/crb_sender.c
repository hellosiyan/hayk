#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "crb_sender.h"
#include "crb_task.h"
#include "crb_channel.h"

static void crb_sender_task_broadcast(crb_task_t *task);

crb_sender_t *
crb_sender_init()
{
    crb_sender_t *sender;

    sender = malloc(sizeof(crb_sender_t));
    if (sender == NULL) {
        return NULL;
    }
    
    sender->tasks = crb_task_queue_init();
    if ( sender->tasks == NULL ) {
		free(sender);
		return NULL;
    }

    return sender;
}

void 
crb_sender_add_task(crb_sender_t *sender, crb_task_t *task)
{
	crb_task_queue_push(sender->tasks, task);
}


static void* 
crb_sender_loop(void *data)
{
	crb_sender_t *sender;
	crb_task_t *task;
	
	sender = (crb_sender_t *) data;
	
	while (1) {
		task = crb_task_queue_pop(sender->tasks);
		if ( task ) {
			switch(task->type) {
				default: 
					crb_sender_task_broadcast(task);
					break;
			}
		} else {
			sleep(1);
		}
	}
	

	return 0;
}

void
crb_sender_run(crb_sender_t *sender)
{
	if ( sender->running ) {
		return;
	}
		
	pthread_create( &(sender->handler), NULL, crb_sender_loop, (void*) sender );
	
}

static void
crb_sender_task_broadcast(crb_task_t *task) {
	crb_channel_t *channel = task->data;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(channel->clients);
	crb_client_t *client;
	
	while ( (client = crb_hash_cursor_next(cursor)) != NULL ) {
		printf("\n\nWRITTEN: %u\n\n", write(client->sock_fd, task->buffer->ptr, task->buffer->used-1));
	}
}

