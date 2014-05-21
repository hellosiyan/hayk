#ifndef __HK_CLIENT_H__
#define __HK_CLIENT_H__ 1

#include <stdint.h>

#include "hk_buffer.h"
#include "hk_request.h"
#include "hk_ws.h"

typedef enum {
     HK_STATE_CONNECTING = 0,
     HK_STATE_OPEN,
     HK_STATE_CLOSING,
     HK_STATE_CLOSED
} hk_client_state_e;

typedef enum {
     HK_DATA_STATE_HANDSHAKE = 0,
     HK_DATA_STATE_FRAME,
     HK_DATA_STATE_FRAME_FRAGMENT
} hk_client_data_state_e;

typedef struct hk_client_s hk_client_t;
struct hk_client_s {
	hk_client_state_e state;
	hk_client_data_state_e data_state;
	int sock_fd;
	uint64_t id;
	
	hk_buffer_t *buffer_in;
	hk_http_request_t *request;

	hk_ws_frame_t *fragmented_frame;
	hk_buffer_t *fragmented_data;
	
	uint32_t ref;
};

hk_client_t *hk_client_init();
void hk_client_ref(hk_client_t *client);
void hk_client_unref(hk_client_t *client);

void hk_client_add_fragment(hk_client_t *client, hk_ws_frame_t *frame);
hk_ws_frame_t *hk_client_compile_fragments(hk_client_t *client);

void hk_client_set_handshake_request(hk_client_t *client, hk_http_request_t *request);
void hk_client_close(hk_client_t *client);
void hk_client_mark_as_closing(hk_client_t *client);

#endif /* __HK_CLIENT_H__ */
