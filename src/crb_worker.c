#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include "crb_worker.h"
#include "crb_client.h"
#include "crb_reader.h"
#include "crb_sender.h"
#include "crb_channel.h"
#include "crb_hash.h"
#include "crb_list.h"

#define SERVER_PORT 8080
#define CRB_WORKER_CHANNELS_SCALE 4

static crb_worker_t *worker;


static void crb_worker_reader_pool_init();
static void crb_worker_sender_pool_init();
static void crb_worker_on_new_client(crb_client_t *client);

void 
crb_worker_create()
{
	worker = malloc(sizeof(crb_worker_t));
	
	worker->channels = crb_hash_init(CRB_WORKER_CHANNELS_SCALE);
	worker->readers = crb_list_init();
	worker->senders = crb_list_init();
	
	worker->active_reader = NULL;
	
	crb_worker_reader_pool_init();
	crb_worker_sender_pool_init();
}

crb_worker_t *
crb_worker_get()
{
	return worker;
}

int 
crb_worker_run()
{
	int sock_desc, new_desc;
	int flags;
	int result;
	struct sockaddr_in address;
	
	crb_client_t *client;
	crb_reader_t* reader = crb_reader_init();
	
	// listen address
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVER_PORT);
	address.sin_addr.s_addr = INADDR_ANY;
	
	sock_desc = socket( AF_INET, SOCK_STREAM, 0 );
	if ( sock_desc == -1 ) {
		perror("socket");
		return -1;
	}
	
	result = bind( sock_desc, (struct sockaddr*)&address, sizeof( struct sockaddr_in ) );
	if ( result == -1 ) {
		perror("bind");
		return -1;
	}
	
	result = listen( sock_desc, 100 );
	if ( result == -1 ) {
		perror("listen");
		return -1;
	}
	
	crb_reader_run(reader);
	
	while (1) {
		new_desc = accept( sock_desc, NULL, NULL);
	
		flags = fcntl (new_desc, F_GETFL, 0);
		if (flags == -1) {
			perror ("fcntl");
			close(new_desc);
			continue;
		}

		flags |= O_NONBLOCK;
		result = fcntl (new_desc, F_SETFL, flags);
		if (result == -1) {
			close(new_desc);
			continue;
		}

		client = crb_client_init();
		client->sock_fd = new_desc;
		
		crb_worker_on_new_client(client);
	}
 	
 	crb_reader_drop_all(reader);
 	close(sock_desc);

	return 0;
}

void
crb_worker_queue_task(crb_task_t *task)
{
	crb_sender_add_task(crb_worker_get()->active_sender, task);
}


int
crb_worker_register_channel(char *name)
{
	crb_worker_t *worker = crb_worker_get();
	crb_channel_t *channel = crb_channel_init();
	
	channel->name = name;
	
	return crb_hash_insert(worker->channels, channel, name, strlen(name));
}

static void
crb_worker_reader_pool_init() {
	crb_worker_t *worker = crb_worker_get();
	crb_reader_t *new_reader;
	int i;
	
	for (i = 0; i < 3; i += 1) {
		new_reader = crb_reader_init();
		crb_reader_run(new_reader);
		crb_list_push(worker->readers, new_reader);
	}
	
	worker->active_reader = (crb_reader_t *)worker->readers->first->data;
}

static void
crb_worker_sender_pool_init() {
	crb_worker_t *worker = crb_worker_get();
	crb_sender_t *new_sender;
	int i;
	
	for (i = 0; i < 3; i += 1) {
		new_sender = crb_sender_init();
		crb_sender_run(new_sender);
		crb_list_push(worker->senders, new_sender);
	}
	
	worker->active_sender = (crb_sender_t *)worker->senders->first->data;
}

static void
crb_worker_on_new_client(crb_client_t *client)
{
	crb_worker_t *worker = crb_worker_get();
	
	crb_reader_add_client(worker->active_reader, client);
}


