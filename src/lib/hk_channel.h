#ifndef __CRB_CHANNEL_H__
#define __CRB_CHANNEL_H__ 1

#include "hk_client.h"
#include "hk_hash.h"

typedef struct hk_channel_s hk_channel_t;
struct hk_channel_s {
	char *name;
	hk_hash_t *clients;
	uint32_t client_count;
};

hk_channel_t *hk_channel_init();
void hk_channel_free(hk_channel_t *channel);

void hk_channel_set_name(hk_channel_t *channel, char *name);
void hk_channel_subscribe(hk_channel_t *channel, hk_client_t *client);
void hk_channel_unsubscribe(hk_channel_t *channel, hk_client_t *client);
hk_client_t *hk_channel_client_is_subscribed(hk_channel_t *channel, hk_client_t *client);

#endif /* __CRB_CHANNEL_H__ */
