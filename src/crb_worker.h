#ifndef __CRB_WORKER_H__
#define __CRB_WORKER_H__ 1

#include "crb_hash.h"
#include "crb_list.h"
#include "crb_channel.h"
#include "crb_reader.h"
#include "crb_sender.h"
#include "crb_channel.h"
#include "crb_config.h"

typedef enum {
     CRB_WORKER_INIT = 0,
     CRB_WORKER_RUNNING,
     CRB_WORKER_STOPPING,
     CRB_WORKER_STOPPED
} crb_worker_state_e;

typedef struct crb_worker_s crb_worker_t;
struct crb_worker_s {
	crb_worker_state_e state;
	int socket_in;
	
	crb_hash_t *channels;
	crb_list_t *readers;
	crb_list_t *senders;
	
	crb_reader_t *active_reader;
	crb_sender_t *active_sender;
	
	crb_config_entry_t *config;
};


void crb_worker_create(crb_config_entry_t *config);
int crb_worker_run();
int crb_worker_stop();
void crb_worker_queue_task();
crb_channel_t *crb_worker_register_channel(char *name);
crb_channel_t *crb_worker_get_channel(char *name, int name_length);
crb_worker_t *crb_worker_get();

void crb_worker_on_client_connect(crb_client_t *client);
void crb_worker_on_client_disconnect(crb_client_t *client);


#endif /* __CRB_WORKER_H__ */
