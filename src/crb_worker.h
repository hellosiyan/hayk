#ifndef __CRB_WORKER_H__
#define __CRB_WORKER_H__ 1

#include "crb_list.h"

typedef struct crb_worker_s crb_worker_t;
struct crb_worker_s {
	crb_list_t *clients;
	crb_list_t *channels;
	crb_list_t *readers;
	crb_list_t *senders;
};

void crb_worker_create();
int crb_worker_run();
crb_worker_t *crb_worker_get();

#endif /* __CRB_WORKER_H__ */
