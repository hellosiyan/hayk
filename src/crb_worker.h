#ifndef __CRB_WORKER_H__
#define __CRB_WORKER_H__ 1

#include "crb_hash.h"
#include "crb_list.h"
#include "crb_reader.h"
#include "crb_sender.h"
#include "crb_channel.h"

typedef struct crb_worker_s crb_worker_t;
struct crb_worker_s {
	crb_hash_t *channels;
	crb_list_t *readers;
	crb_list_t *senders;
	
	crb_reader_t *active_reader;
	crb_sender_t *active_sender;
};

void crb_worker_create();
int crb_worker_run();
void crb_worker_queue_task();
crb_channel_t * crb_worker_register_channel(char *name);
crb_worker_t *crb_worker_get();

#endif /* __CRB_WORKER_H__ */
