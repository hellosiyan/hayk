#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crb_atomic.h"
#include "crb_channel.h"
#include "crb_hash.h"

static void crb_hash_item_add_client(crb_hash_item_t *item);
static void crb_hash_item_remove_client(crb_hash_item_t *item);

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
    
	channel->clients = crb_hash_init(4);
	channel->clients->item_add = crb_hash_item_add_client;
	channel->clients->item_remove = crb_hash_item_remove_client;

    return channel;
}

void 
crb_channel_free(crb_channel_t *channel)
{	
	if ( channel == NULL ) {
		return;
	}
	
	/* Free clients pool */
	crb_client_t *client;
	crb_hash_cursor_t *cursor = crb_hash_cursor_init(channel->clients);

	while ( (client = crb_hash_cursor_next(cursor)) != NULL ) {
		crb_client_unref(client);
	}

	crb_hash_cursor_free(cursor);
	crb_hash_free(channel->clients);
	channel->clients = NULL;
	
	free(channel->name);
	channel->name = NULL;
	
	free(channel);
}

void 
crb_channel_set_name(crb_channel_t *channel, char *name)
{
	size_t length;
	
	if ( channel == NULL || name == NULL ) {
		return;
	}
	
	if ( channel->name ) {
		free(channel->name);
	}
	
	length = strlen(name)+1;
	channel->name = malloc(length);
	strcpy(channel->name, name);
	channel->name[length-1] = '\0';
}

void 
crb_channel_subscribe(crb_channel_t *channel, crb_client_t *client)
{
	if ( channel == NULL || client == NULL ) {
		return;
	}
	
	crb_hash_insert(channel->clients, client, &(client->id), sizeof(int));
	crb_client_ref(client);
	crb_atomic_fetch_add( &(channel->client_count), 1 );
}

void 
crb_channel_unsubscribe(crb_channel_t *channel, crb_client_t *client)
{
	if ( channel == NULL || client == NULL ) {
		return;
	}
	
	crb_hash_remove(channel->clients, &(client->id), sizeof(int));
	
	crb_client_unref(client);
	crb_atomic_fetch_sub(&(channel->client_count), 1);
}

static void
crb_hash_item_add_client(crb_hash_item_t *item)
{
	crb_client_ref((crb_client_t*) item->data);
}


static void
crb_hash_item_remove_client(crb_hash_item_t *item)
{
	crb_client_unref((crb_client_t*) item->data);
	crb_hash_item_unref(item);
}

