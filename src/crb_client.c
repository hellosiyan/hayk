#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>

#include "crb_atomic.h"
#include "crb_ws.h"
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
    client->data_state = CRB_STATE_CONNECTING;
    client->request = NULL;
    
    client->buffer_in = crb_buffer_init(CRB_READER_BUFFER_SIZE);
    if ( client->buffer_in == NULL ) {
    	crb_log_error("Cannot allocate client buffer");
    	free(client);
    	return NULL;
    }

	client->fragmented_frame = NULL;
	client->fragmented_data = crb_buffer_init(CRB_READER_BUFFER_SIZE);
	if ( client->fragmented_data == NULL ) {
    	crb_log_error("Cannot allocate fragment data buffer");
    	free(client);
    	return NULL;
    }	
    
    client->id = 0;
    
    client->ref = 1;

    return client;
}

void 
crb_client_set_request(crb_client_t *client, crb_request_t *request)
{
	if ( client == NULL ) {
		return;
	}
	
	if ( client->request != NULL ) {
		crb_request_unref(client->request);
	}
	
	if ( request != NULL ) {
		crb_request_ref(request);
	}
	client->request = request;
}

void 
crb_client_add_fragment(crb_client_t *client, crb_ws_frame_t *frame)
{
	if ( client->fragmented_frame == NULL ) {
		client->fragmented_frame = crb_ws_frame_init();

		*(client->fragmented_frame) = *frame;
		client->fragmented_frame->data = NULL;
		client->fragmented_frame->data_length = 0;
	}

	crb_buffer_append_string(client->fragmented_data, frame->data, frame->data_length);
}

crb_ws_frame_t *
crb_client_get_fragments_as_frame(crb_client_t *client)
{
	crb_ws_frame_t *frame;

	frame = crb_ws_frame_init();
	*frame = *(client->fragmented_frame);
	frame->data = crb_buffer_copy_string_without_sentinel(client->fragmented_data, &(frame->data_length));

	// free client fragmented data
	crb_ws_frame_free_with_data(client->fragmented_frame);
	client->fragmented_frame = NULL;

	crb_buffer_clear(client->fragmented_data);

	return frame;
}

inline void 
crb_client_ref(crb_client_t *client)
{
	crb_atomic_fetch_add( &(client->ref), 1 );
}

inline void 
crb_client_unref(crb_client_t *client)
{
	uint32_t old_ref;
	old_ref = crb_atomic_sub_fetch( &(client->ref), 1 );
	if ( old_ref == 0 ) {
		crb_client_free(client);
	}
}

void 
crb_client_mark_as_closing(crb_client_t *client) 
{
	if ( client == NULL || client->state == CRB_STATE_CLOSED || client->state == CRB_STATE_CLOSING ) {
		return;
	}
	
	client->state = CRB_STATE_CLOSING;
}

void 
crb_client_close(crb_client_t *client) 
{
	uint8_t *close_frame;
	int close_frame_length;

	if ( client == NULL || client->state == CRB_STATE_CLOSED ) {
		return;
	}
	
	client->state = CRB_STATE_CLOSING;

	close_frame = crb_ws_frame_close(&close_frame_length, 0);

	write(client->sock_fd, (char*)close_frame, close_frame_length);
	close(client->sock_fd);

	client->state = CRB_STATE_CLOSED;

	free(close_frame);
	crb_log_info("Closed client");
}


static void 
crb_client_free(crb_client_t *client)
{
	if ( client == NULL ) {
		return;
	}
	
	crb_client_close(client);
	
	crb_buffer_free(client->buffer_in);
	client->buffer_in = NULL;
	
	if ( client->request != NULL ) {		
		crb_request_free(client->request);
		client->request = NULL;
	}

	if ( client->fragmented_frame != NULL ) {
		crb_ws_frame_free_with_data(client->fragmented_frame);
		client->fragmented_frame = NULL;
	}

	crb_buffer_free(client->fragmented_data);
	client->fragmented_data = NULL;
	
	free(client);
}
