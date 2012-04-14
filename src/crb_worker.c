#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
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
static void _crb_worker_stop();

void 
crb_worker_create()
{
	worker = malloc(sizeof(crb_worker_t));
	
	worker->state = CRB_WORKER_STOPPED;
	
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
	int on = 1;
	struct sockaddr_in address;
	
	worker->state = CRB_WORKER_INIT;
	
	crb_client_t *client;
	crb_reader_t* reader = worker->active_reader;
	
	// listen address
	address.sin_family = AF_INET;
	address.sin_port = htons(SERVER_PORT);
	address.sin_addr.s_addr = INADDR_ANY;
	
	sock_desc = socket( AF_INET, SOCK_STREAM, 0 );
	worker->socket_in = sock_desc;
	if ( sock_desc == -1 ) {
		perror("socket");
		worker->state = CRB_WORKER_STOPPED;
		return -1;
	}
	
	result = setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
	result = bind( sock_desc, (struct sockaddr*)&address, sizeof( struct sockaddr_in ) );
	if ( result == -1 ) {
		perror("bind");
		worker->state = CRB_WORKER_STOPPED;
		return -1;
	}
	
	result = listen( sock_desc, 100 );
	if ( result == -1 ) {
		perror("listen");
		worker->state = CRB_WORKER_STOPPED;
		return -1;
	}
	
	flags = fcntl (sock_desc, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		worker->state = CRB_WORKER_STOPPED;
		return -1;
	}

	flags |= O_NONBLOCK;
	result = fcntl (sock_desc, F_SETFL, flags);
	if (result == -1) {
		perror ("fcntl");
		worker->state = CRB_WORKER_STOPPED;
		return -1;
	}
	
	worker->state = CRB_WORKER_RUNNING;
	
	while (worker->state == CRB_WORKER_RUNNING) {
		new_desc = accept( sock_desc, NULL, NULL);
		if ( new_desc < 0 ) {
			sleep(1);
			continue;
		}
	
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
 	
 	_crb_worker_stop();

	return 0;
}

int 
crb_worker_stop()
{
	worker->state = CRB_WORKER_STOPPING;
}

static void
_crb_worker_stop()
{
	crb_list_item_t *item;
	crb_reader_t *reader;
	
	item = worker->readers->first;
	while ( item != NULL ) {
		reader = (crb_reader_t *)item->data;
		crb_reader_stop(reader);
		item = item->next;
		pthread_join(reader->thread_id, NULL);
	}
	
	worker->state = CRB_WORKER_STOPPED;
	
	close(worker->socket_in);
}

void
crb_worker_queue_task(crb_task_t *task)
{
	crb_sender_add_task(crb_worker_get()->active_sender, task);
}


crb_channel_t *
crb_worker_register_channel(char *name)
{
	crb_worker_t *worker = crb_worker_get();
	crb_channel_t *channel = crb_channel_init();
	
	channel->name = name;
	
	channel = crb_hash_insert(worker->channels, channel, name, strlen(name));
	
	return channel;
}

static void
crb_worker_on_new_client(crb_client_t *client)
{
	crb_worker_t *worker = crb_worker_get();
	
	crb_reader_add_client(worker->active_reader, client);
	
	/* TODO: remove; begin test code */
	crb_channel_add_client(crb_worker_register_channel("test"), client);
	/* end test code */
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


