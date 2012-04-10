#include <stdio.h>
#include <stdlib.h>

#include "crb_client.h"

#define CRB_READER_BUFFER_SIZE 4096

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

    return client;
}

void 
crb_client_close(crb_client_t *client) 
{
	close(client->sock_fd);
	crb_buffer_free(client->buffer_in);
}

