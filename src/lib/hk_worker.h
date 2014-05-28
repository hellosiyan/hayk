#ifndef __HK_WORKER_H__
#define __HK_WORKER_H__ 1

#include "hk_hash.h"
#include "hk_list.h"
#include "hk_reader.h"
#include "hk_config.h"

typedef enum {
     HK_WORKER_INIT = 0,
     HK_WORKER_RUNNING,
     HK_WORKER_STOPPING,
     HK_WORKER_STOPPED
} hk_worker_state_e;

typedef struct hk_worker_s hk_worker_t;
struct hk_worker_s {
	pid_t pid;
	hk_worker_state_e state;
	int socket_in;
	
	hk_list_t *readers;
	
	hk_reader_t *active_reader;
	
	hk_config_entry_t *config;

	unsigned is_forked:1;
};


hk_worker_t * hk_worker_create(hk_config_entry_t *config);
int hk_worker_fork_and_run(hk_worker_t * worker);
int hk_worker_run(hk_worker_t * worker);
void hk_worker_stop(hk_worker_t * worker);
hk_worker_t *hk_worker_get();

void hk_worker_on_client_connect(hk_client_t *client);
void hk_worker_on_client_disconnect(hk_client_t *client);


#endif /* __HK_WORKER_H__ */
