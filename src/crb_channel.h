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
};

crb_channel_t *crb_channel_init();
void crb_channel_free(crb_channel_t *channel);
void crb_channel_set_name(crb_channel_t *channel, char *name);
void crb_channel_subscribe(crb_channel_t *channel, crb_client_t *client);
void crb_channel_unsubscribe(crb_channel_t *channel, crb_client_t *client);
void crb_channel_free(crb_channel_t *channel);

#endif /* __CRB_CHANNEL_H__ */
