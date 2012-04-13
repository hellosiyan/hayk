#ifndef __CRB_CHANNEL_H__
#define __CRB_CHANNEL_H__ 1

#include "crb_task.h"
#include "crb_client.h"
#include "crb_hash.h"

#define CRB_CHANNEL_MAX_CLIENTS 10

typedef struct crb_channel_s crb_channel_t;

struct crb_channel_s {
	char *name;
	crb_hash_t *clients;
	int client_count;
	crb_task_queue_t *tasks;
};

crb_channel_t *crb_channel_init();
void crb_channel_add_client(crb_channel_t *channel, crb_client_t *client);
void crb_channel_add_task(crb_channel_t *channel, crb_task_t *task);

#endif /* __CRB_CHANNEL_H__ */
