#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

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
    
    sender->running = 0;
    
    sender->tasks = crb_task_queue_init();
    if ( sender->tasks == NULL ) {
		free(sender);
		return NULL;
    }
    
    sender->mu_tasks = malloc(sizeof(pthread_mutex_t));
    if ( sender->mu_tasks == NULL ) {
		free(sender->tasks);
    	free(sender);
		return NULL;
    }
    pthread_mutex_init(sender->mu_tasks, NULL);
    
    sem_init(&sender->sem_tasks, 0, 0);
    
    return sender;
}

void 
crb_sender_free(crb_sender_t *sender)
{
	crb_sender_stop(sender);
	
	pthread_mutex_lock(sender->mu_tasks);
	crb_task_queue_free(sender->tasks);
	pthread_mutex_unlock(sender->mu_tasks);
	
	pthread_mutex_destroy(sender->mu_tasks);
	free(sender->mu_tasks);
	sem_destroy(&sender->sem_tasks);
	
	free(sender);
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
	int result;
	
	sender = (crb_sender_t *) data;
	
	sender->running = 1;
	while (sender->running) {
		result = sem_wait(&sender->sem_tasks);
		
		pthread_mutex_lock(sender->mu_tasks);
		task = crb_task_queue_pop(sender->tasks);
		pthread_mutex_unlock(sender->mu_tasks);
		
		if ( task ) {
			switch(task->type) {
				case CRB_TASK_SHUTDOWN:
					// breaks out of the loop
					sender->running = 0;
					crb_task_free(task);
					break;
				case CRB_TASK_BROADCAST:
					crb_sender_task_broadcast(task);
					break;
				default: 
					break;
			}
		}
	}
	
	printf("sender closed\n");

	return 0;
}

void
crb_sender_run(crb_sender_t *sender)
{
	if ( sender->running ) {
		return;
	}
		
	pthread_create( &(sender->thread_id), NULL, crb_sender_loop, (void*) sender );
	
}

void
crb_sender_stop(crb_sender_t *sender)
{
	crb_task_t *task;
	
	if ( !sender->running ) {
		return;
	}
	
	task = crb_task_init();
	task->type = CRB_TASK_SHUTDOWN;
	
	pthread_mutex_lock(sender->mu_tasks);
	crb_sender_add_task(sender, task);
	pthread_mutex_unlock(sender->mu_tasks);
	sem_post(&sender->sem_tasks);
	
	pthread_join(sender->thread_id, NULL);
	
	sender->running = 0;
}

static void
crb_sender_task_broadcast(crb_task_t *task) {
	crb_channel_t *channel = task->data;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(channel->clients);
	crb_client_t *client;
	
	while ( (client = crb_hash_cursor_next(cursor)) != NULL ) {
		if ( client->state == CRB_STATE_OPEN && client->sock_fd != task->client->sock_fd ) {
			write(client->sock_fd, task->buffer->ptr, task->buffer->used-1);
		}
	}
	
	crb_hash_cursor_free(cursor);
	crb_buffer_free(task->buffer);
	crb_task_free(task);
}

