#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hk_atomic.h"
#include "hk_channel.h"
#include "hk_hash.h"

static void hk_hash_item_add_client(hk_hash_item_t *item);
static void hk_hash_item_remove_client(hk_hash_item_t *item);

hk_channel_t *
hk_channel_init()
{
    hk_channel_t *channel;

    channel = malloc(sizeof(hk_channel_t));
    if (channel == NULL) {
        return NULL;
    }
    
    channel->name = NULL;
    channel->client_count = 0;
    
	channel->clients = hk_hash_init(4);
	channel->clients->item_add = hk_hash_item_add_client;
	channel->clients->item_remove = hk_hash_item_remove_client;

    return channel;
}

void 
hk_channel_free(hk_channel_t *channel)
{	
	if ( channel == NULL ) {
		return;
	}
	
	/* Free clients pool */
	hk_client_t *client;
	hk_hash_cursor_t *cursor = hk_hash_cursor_init(channel->clients);

	while ( (client = hk_hash_cursor_next(cursor)) != NULL ) {
		hk_client_unref(client);
	}

	hk_hash_cursor_free(cursor);
	hk_hash_free(channel->clients);
	channel->clients = NULL;
	
	free(channel->name);
	channel->name = NULL;
	
	free(channel);
}

void 
hk_channel_set_name(hk_channel_t *channel, char *name)
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
	if ( channel->name == NULL ) {
		hk_log_error("Cannot allocate memory for channel name");
		return;
	}
	
	strcpy(channel->name, name);
	channel->name[length-1] = '\0';
}

void 
hk_channel_subscribe(hk_channel_t *channel, hk_client_t *client)
{
	if ( channel == NULL || client == NULL || hk_channel_client_is_subscribed(channel, client) ) {
		return;
	}
	
	hk_hash_insert(channel->clients, client, &(client->id), sizeof(int));
	hk_client_ref(client);
	hk_atomic_fetch_add( &(channel->client_count), 1 );
}

void 
hk_channel_unsubscribe(hk_channel_t *channel, hk_client_t *client)
{
	if ( channel == NULL || client == NULL || !hk_channel_client_is_subscribed(channel, client) ) {
		return;
	}
	
	hk_hash_remove(channel->clients, &(client->id), sizeof(int));
	
	hk_client_unref(client);
	hk_atomic_sub_fetch(&(channel->client_count), 1);
}

hk_client_t *
hk_channel_client_is_subscribed(hk_channel_t *channel, hk_client_t *client)
{
	return (hk_client_t *) hk_hash_exists_key(channel->clients, &(client->id), sizeof(int));
}

static void
hk_hash_item_add_client(hk_hash_item_t *item)
{
	hk_client_ref((hk_client_t*) item->data);
}


static void
hk_hash_item_remove_client(hk_hash_item_t *item)
{
	hk_client_unref((hk_client_t*) item->data);
	hk_hash_item_unref(item);
}

