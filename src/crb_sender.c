#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>


#include "crb_sender.h"
#include "crb_task.h"

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
			write(STDOUT_FILENO, "task!\n", 6);
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
