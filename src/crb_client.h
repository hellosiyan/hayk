#ifndef __CRB_CLIENT_H__
#define __CRB_CLIENT_H__ 1

#include "crb_buffer.h"


typedef enum {
     CRB_STATE_CONNECTING = 0,
     CRB_STATE_OPEN,
     CRB_STATE_CLOSING,
     CRB_STATE_CLOSED
} crb_client_state_e;

typedef struct crb_client_s crb_client_t;

struct crb_client_s {
	crb_client_state_e state;
	int sock_fd;
	int id;
	
	crb_buffer_t *buffer_in;
};

crb_client_t *crb_client_init();

#endif /* __CRB_CLIENT_H__ */
