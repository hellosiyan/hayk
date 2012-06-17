#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>

#include "crb_atomic.h"
#include "crb_client.h"

#define CRB_READER_BUFFER_SIZE 4096

static void crb_client_free(crb_client_t *client);

crb_client_t *
crb_client_init()
{
    crb_client_t *client;

    client = malloc(sizeof(crb_client_t));
    if (client == NULL) {
        return NULL;
    }
    
    client->state = CRB_STATE_CONNECTING;
    client->buffer_in = crb_buffer_init(CRB_READER_BUFFER_SIZE);
    if ( client->buffer_in == NULL ) {
    	free(client);
    	return NULL;
    }
    
    client->id = 0;
    
    client->ref = 1;

    return client;
}

void 
crb_client_ref(crb_client_t *client)
{
	crb_atomic_fetch_add( &(client->ref), 1 );
}

void 
crb_client_unref(crb_client_t *client)
{
	int old_ref;
	old_ref = __sync_sub_and_fetch( &(client->ref), 1 );
	if ( old_ref == 0 ) {
		crb_client_free(client);
	}
}

void 
crb_client_close(crb_client_t *client) 
{
	client->state = CRB_STATE_CLOSING;
	close(client->sock_fd);
	client->state = CRB_STATE_CLOSED;
}


static void 
crb_client_free(crb_client_t *client)
{
	crb_client_close(client);
	crb_buffer_free(client->buffer_in);
	free(client);
}
