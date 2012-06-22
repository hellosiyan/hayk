#define _GNU_SOURCE

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
#include <signal.h>
#include <poll.h>

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


typedef struct {
	int signo;
	void (*handler)(int signo);
} crb_signal_t;


crb_signal_t  signals[] = {
    { SIGPIPE, SIG_IGN },
    { 0, NULL }
};


static void crb_worker_signals_init();
static void crb_worker_reader_pool_init();
static void crb_worker_sender_pool_init();
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
	
	crb_worker_signals_init();
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
	struct pollfd pfd; 
	
	worker->state = CRB_WORKER_INIT;
	
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
	
	pfd.fd = sock_desc;
	pfd.events = POLLIN | POLLOUT | POLLHUP; 
	
	worker->state = CRB_WORKER_RUNNING;
	
	while (worker->state == CRB_WORKER_RUNNING) {
		poll(&pfd, 1, -1);
		new_desc = accept( sock_desc, NULL, NULL);
		if ( new_desc < 0 ) {
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

		{
			crb_client_t *client;
			client = crb_client_init();
			client->sock_fd = new_desc;
		
			crb_worker_on_client_connect(client);
		}
	}
 	
 	_crb_worker_stop();

	return 0;
}

int 
crb_worker_stop()
{
	worker->state = CRB_WORKER_STOPPING;
	shutdown(worker->socket_in, SHUT_RDWR);
}

static void
_crb_worker_stop()
{
	close(worker->socket_in);
	
	{
		/* Stop and close readers */
		crb_list_item_t *item;
		crb_reader_t *reader;
	
		reader = (crb_reader_t *) crb_list_pop(worker->readers);
		while ( reader != NULL ) {
			crb_reader_stop(reader);
			crb_reader_free(reader);
			reader = (crb_reader_t *) crb_list_pop(worker->readers);
		}
	}
	
	{
		/* Stop and close senders */
		crb_list_item_t *item;
		crb_sender_t *sender;
		
		sender = (crb_sender_t *) crb_list_pop(worker->senders); // worker->senders->first;
		while ( sender != NULL ) {
			crb_sender_stop(sender);
			crb_sender_free(sender);
			sender = (crb_sender_t *) crb_list_pop(worker->senders);
		}
	}
	
	worker->state = CRB_WORKER_STOPPED;
	
	{
		/* Free channel pool */
		crb_channel_t *channel;
		crb_hash_cursor_t *cursor = crb_hash_cursor_init(worker->channels);
	
		while ( (channel = crb_hash_cursor_next(cursor)) != NULL ) {
			crb_channel_free(channel);
		}
	
		crb_hash_cursor_free(cursor);
		crb_hash_free(worker->channels);
	}
}

void
crb_worker_queue_task(crb_task_t *task)
{
	crb_sender_t *sender = crb_worker_get()->active_sender;
	
	pthread_mutex_lock(sender->mu_tasks);
	crb_sender_add_task(sender, task);
	pthread_mutex_unlock(sender->mu_tasks);
	sem_post(&sender->sem_tasks);
}

crb_channel_t *
crb_worker_register_channel(char *name)
{
	crb_worker_t *worker = crb_worker_get();
	crb_channel_t *channel;
	
	channel = crb_hash_exists_key(worker->channels, name, strlen(name));
	
	if ( channel == NULL ) {
		printf("CREATE \"%s\"\n", name);
		channel = crb_channel_init();
		crb_channel_set_name(channel, name);
		channel = crb_hash_insert(worker->channels, channel, name, strlen(name));
	}
	
	return channel;
}

crb_channel_t *
crb_worker_get_channel(char *name, int name_length)
{
	crb_worker_t *worker = crb_worker_get();
	
	if ( name_length == -1 ) {
		name_length = strlen(name);
	}
	
	return crb_hash_exists_key(worker->channels, name, name_length);
}

void
crb_worker_on_client_connect(crb_client_t *client)
{
	client->state = CRB_STATE_CONNECTING;
	crb_reader_add_client(worker->active_reader, client);
}

void
crb_worker_on_client_disconnect(crb_client_t *client)
{
	crb_channel_t *channel;
	crb_hash_cursor_t *cursor;
	
	if ( client == NULL ) {
		return;
	}
	
	/* Unsubscribe client from all channels */
	cursor = crb_hash_cursor_init(worker->channels);

	while ( (channel = crb_hash_cursor_next(cursor)) != NULL ) {
		crb_channel_unsubscribe(channel, client);
	}

	crb_hash_cursor_free(cursor);
}


static void 
crb_worker_signals_init()
{
    crb_signal_t      *sig;
    struct sigaction   sa;

    for (sig = signals; sig->signo != 0; sig++) {
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            break;
        }
    }
}

static void
crb_worker_reader_pool_init()
{
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
crb_worker_sender_pool_init()
{
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
