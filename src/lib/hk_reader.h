#ifndef __CRB_READER_H__
#define __CRB_READER_H__ 1

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hk_worker.h"
#include "hk_hash.h"
#include "hk_client.h"
#include "hk_channel.h"
#include "hk_ws.h"

#define CRB_READER_CLIENTS_SCALE 4
#define CRB_READER_EPOLL_MAX_EVENTS 20

typedef struct hk_reader_s hk_reader_t;
struct hk_reader_s {
	pthread_t thread_id;
	hk_hash_t *clients;
	uint32_t client_count;
	uint32_t cid;
	int epoll_fd;
	unsigned running:1;
};

hk_reader_t *hk_reader_init();
void hk_reader_free(hk_reader_t *reader);

void hk_reader_run(hk_reader_t *reader);
void hk_reader_stop(hk_reader_t *reader);
void hk_reader_add_client(hk_reader_t *reader, hk_client_t *client);
void hk_reader_drop_client(hk_reader_t *reader, hk_client_t *client);
void hk_reader_drop_all_clients(hk_reader_t *reader);

#endif /* __CRB_READER_H__ */
