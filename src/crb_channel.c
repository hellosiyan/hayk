#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crb_channel.h"
#include "crb_hash.h"

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
    
	channel->clients = crb_hash_init(16);

    return channel;
}

void 
crb_channel_free(crb_channel_t *channel)
{	
	free(channel->name);
	free(channel);
}

void 
crb_channel_set_name(crb_channel_t *channel, char *name)
{
	if ( channel->name ) {
		free(channel->name);
	}
	channel->name = malloc(strlen(name)+1);
	strcpy(channel->name, name);
}

void 
crb_channel_add_client(crb_channel_t *channel, crb_client_t *client)
{
	crb_hash_insert(channel->clients, client, &(client->sock_fd), sizeof(int));
	channel->client_count++;
}

