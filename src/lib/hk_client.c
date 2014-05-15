#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>

#include "hk_atomic.h"
#include "hk_ws.h"
#include "hk_client.h"

#define CRB_READER_BUFFER_SIZE 4096

static void hk_client_free(hk_client_t *client);

hk_client_t *
hk_client_init()
{
    hk_client_t *client;

    client = malloc(sizeof(hk_client_t));
    if (client == NULL) {
        return NULL;
    }
    
    client->state = CRB_STATE_CONNECTING;
    client->data_state = CRB_STATE_CONNECTING;
    client->request = NULL;
    
    client->buffer_in = hk_buffer_init(CRB_READER_BUFFER_SIZE);
    if ( client->buffer_in == NULL ) {
    	hk_log_error("Cannot allocate client buffer");
    	free(client);
    	return NULL;
    }

	client->fragmented_frame = NULL;
	client->fragmented_data = hk_buffer_init(CRB_READER_BUFFER_SIZE);
	if ( client->fragmented_data == NULL ) {
    	hk_log_error("Cannot allocate fragment data buffer");
    	free(client);
    	return NULL;
    }	
    
    client->id = 0;
    
    client->ref = 1;

    return client;
}

void 
hk_client_set_handshake_request(hk_client_t *client, hk_http_request_t *request)
{
	if ( client == NULL ) {
		return;
	}
	
	if ( client->request != NULL ) {
		hk_request_unref(client->request);
	}
	
	if ( request != NULL ) {
		hk_request_ref(request);
	}
	client->request = request;
}

void 
hk_client_add_fragment(hk_client_t *client, hk_ws_frame_t *frame)
{
	if ( client->fragmented_frame == NULL ) {
		client->fragmented_frame = hk_ws_frame_init();

		*(client->fragmented_frame) = *frame;
		client->fragmented_frame->data = NULL;
		client->fragmented_frame->data_length = 0;
	}

	client->fragmented_frame->utf8_state = frame->utf8_state;

	hk_buffer_append_string(client->fragmented_data, frame->data, frame->data_length);
}

hk_ws_frame_t *
hk_client_compile_fragments(hk_client_t *client)
{
	hk_ws_frame_t *frame;

	frame = hk_ws_frame_init();
	*frame = *(client->fragmented_frame);
	frame->data = hk_buffer_copy_string_without_sentinel(client->fragmented_data, &(frame->data_length));

	// free client fragmented data
	hk_ws_frame_free_with_data(client->fragmented_frame);
	client->fragmented_frame = NULL;

	hk_buffer_clear(client->fragmented_data);

	return frame;
}

inline void 
hk_client_ref(hk_client_t *client)
{
	hk_atomic_fetch_add( &(client->ref), 1 );
}

inline void 
hk_client_unref(hk_client_t *client)
{
	uint32_t old_ref;
	old_ref = hk_atomic_sub_fetch( &(client->ref), 1 );
	if ( old_ref == 0 ) {
		hk_client_free(client);
	}
}

void 
hk_client_mark_as_closing(hk_client_t *client) 
{
	if ( client == NULL || client->state == CRB_STATE_CLOSED || client->state == CRB_STATE_CLOSING ) {
		return;
	}
	
	client->state = CRB_STATE_CLOSING;
}

void 
hk_client_close(hk_client_t *client) 
{
	uint8_t *close_frame;
	int close_frame_length;

	if ( client == NULL || client->state == CRB_STATE_CLOSED ) {
		return;
	}
	
	client->state = CRB_STATE_CLOSING;

	close_frame = hk_ws_generate_close_frame(&close_frame_length, 0);

	write(client->sock_fd, (char*)close_frame, close_frame_length);
	close(client->sock_fd);

	client->state = CRB_STATE_CLOSED;

	free(close_frame);
	hk_log_info("Closed client");
}


static void 
hk_client_free(hk_client_t *client)
{
	if ( client == NULL ) {
		return;
	}
	
	hk_client_close(client);
	
	hk_buffer_free(client->buffer_in);
	client->buffer_in = NULL;
	
	if ( client->request != NULL ) {		
		hk_request_unref(client->request);
		client->request = NULL;
	}

	if ( client->fragmented_frame != NULL ) {
		hk_ws_frame_free_with_data(client->fragmented_frame);
		client->fragmented_frame = NULL;
	}

	hk_buffer_free(client->fragmented_data);
	client->fragmented_data = NULL;
	
	free(client);
}
