#ifndef __HK_SENDER_H__
#define __HK_SENDER_H__ 1

#include <pthread.h>
#include <semaphore.h>

#include "hk_task.h"

#define HK_SENDER_MAX_TASKS 10

typedef struct hk_sender_s hk_sender_t;
struct hk_sender_s {
	pthread_t thread_id;
	
	hk_task_queue_t *tasks;
	pthread_mutex_t *mu_tasks;
	sem_t sem_tasks;
	
	unsigned running:1;
};

hk_sender_t *hk_sender_init();
void hk_sender_free(hk_sender_t *sender);

void hk_sender_run(hk_sender_t *sender);
void hk_sender_stop(hk_sender_t *sender);
void hk_sender_add_task(hk_sender_t *sender, hk_task_t *task);

#endif /* __HK_READER_H__ */
