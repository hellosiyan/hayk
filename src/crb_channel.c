#include <stdlib.h>


#include "crb_channel.h"

crb_channel_t *
crb_channel_init()
{
    crb_channel_t *channel;

    channel = malloc(sizeof(crb_channel_t));
    if (channel == NULL) {
        return NULL;
    }
    
    channel->name = NULL;
    channel->client_count = 0;
    
    channel->tasks = crb_task_queue_init();
    if ( channel->tasks == NULL ) {
		free(channel);
		return NULL;
    }

    return channel;
}

void 
crb_channel_add_client(crb_channel_t *channel, crb_client_t *client)
{
	
	channel->clients[channel->client_count] = client;
	channel->client_count++;
}

void 
crb_channel_add_task(crb_channel_t *channel, crb_task_t *task)
{
	crb_task_queue_push(channel->tasks, task);
}
