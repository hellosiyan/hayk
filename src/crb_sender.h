#ifndef __CRB_SENDER_H__
#define __CRB_SENDER_H__ 1

#include <pthread.h>
#include <semaphore.h>

#include "crb_task.h"

#define CRB_SENDER_MAX_TASKS 10

typedef struct crb_sender_s crb_sender_t;

struct crb_sender_s {
	pthread_t thread_id;
	
	crb_task_queue_t *tasks;
	pthread_mutex_t *mu_tasks;
	sem_t sem_tasks;
	
	unsigned running:1;
};


crb_sender_t *crb_sender_init();
void crb_sender_run(crb_sender_t *sender);
void crb_sender_stop(crb_sender_t *sender);
void crb_reader_add_task(crb_sender_t *sender, crb_task_t *task);

#endif /* __CRB_READER_H__ */
