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

#include "hk_worker.h"
#include "hk_client.h"
#include "hk_reader.h"
#include "hk_sender.h"
#include "hk_hash.h"
#include "hk_list.h"


#define SERVER_PORT 8080


static hk_worker_t *worker_inst;

static void hk_worker_signals_init();
static void hk_worker_reader_pool_init();
static void hk_worker_sender_pool_init();
static void _hk_worker_stop();
static void hk_worker_sig(int signo);


typedef struct {
	int signo;
	void (*handler)(int signo);
} hk_signal_t;

static hk_signal_t  signals[] = {
    { SIGINT, hk_worker_sig },
    { SIGPIPE, SIG_IGN },
    { 0, NULL }
};


hk_worker_t *
hk_worker_create(hk_config_entry_t *config)
{
	hk_worker_t * worker;
	worker = malloc(sizeof(hk_worker_t));
	
	worker->state = HK_WORKER_STOPPED;
	
	worker->readers = hk_list_init();
	worker->senders = hk_list_init();
	
	worker->active_reader = NULL;
	
	worker->config = config;
	
	worker->pid = 0;
	worker->socket_in = 0;
	worker->is_forked = 0;
	
	return worker;
}

hk_worker_t *
hk_worker_get()
{
	return worker_inst;
}

int 
hk_worker_fork_and_run(hk_worker_t * worker)
{
	pid_t pid;

	if ( worker->state != HK_WORKER_STOPPED ) {
		hk_log_error("Cannot run, worker already running");
		return 0;
	}

	pid = fork();
	worker->is_forked = 1;
	
	if ( pid != 0 ) {
		return pid;
	}
	
	worker->pid = pid;
	
	hk_worker_signals_init();

	hk_worker_run(worker);
}

int 
hk_worker_run(hk_worker_t * worker)
{
	worker_inst = worker;

	if ( worker->state != HK_WORKER_STOPPED ) {
		hk_log_error("Cannot run, worker already running");
		return 0;
	}

	hk_worker_reader_pool_init();
	hk_worker_sender_pool_init();
	
	int sock_desc, new_desc;
	int flags;
	int result;
	int on = 1;
	struct sockaddr_in address;
	struct pollfd pfd; 
	
	worker->state = HK_WORKER_INIT;
	
	hk_reader_t* reader = worker->active_reader;
	
	// listen address
	address.sin_family = AF_INET;
	address.sin_port = htons( worker->config->port );
	address.sin_addr.s_addr = worker->config->host.s_addr;
	
	sock_desc = socket( AF_INET, SOCK_STREAM, 0 );
	worker->socket_in = sock_desc;
	if ( sock_desc == -1 ) {
		perror("socket");
		worker->state = HK_WORKER_STOPPED;
		exit(EXIT_FAILURE);
	}
	
	result = setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
	result = bind( sock_desc, (struct sockaddr*)&address, sizeof( struct sockaddr_in ) );
	if ( result == -1 ) {
		perror("bind");
		worker->state = HK_WORKER_STOPPED;
		exit(EXIT_FAILURE);
	}
	
	result = listen( sock_desc, 100 );
	if ( result == -1 ) {
		perror("listen");
		worker->state = HK_WORKER_STOPPED;
		exit(EXIT_FAILURE);
	}
	
	flags = fcntl (sock_desc, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		worker->state = HK_WORKER_STOPPED;
		exit(EXIT_FAILURE);
	}

	flags |= O_NONBLOCK;
	result = fcntl (sock_desc, F_SETFL, flags);
	if (result == -1) {
		perror ("fcntl");
		worker->state = HK_WORKER_STOPPED;
		exit(EXIT_FAILURE);
	}
	
	pfd.fd = sock_desc;
	pfd.events = POLLIN | POLLOUT | POLLHUP; 
	
	worker->state = HK_WORKER_RUNNING;
	
	while (worker->state == HK_WORKER_RUNNING) {
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
			perror ("fcntl");
			close(new_desc);
			continue;
		}

		{
			hk_log_info("Add client");
			hk_client_t *client;
			client = hk_client_init();
			client->sock_fd = new_desc;
		
			hk_worker_on_client_connect(client);
		}
	}

	printf("exiting .. \n");
 	_hk_worker_stop();

 	if ( worker->is_forked ) {
 		exit(0);
 	}
}

int 
hk_worker_stop(hk_worker_t * worker)
{
	if ( worker != NULL && worker->is_forked && worker->pid != 0 ) {
		kill(worker->pid, SIGINT);
		waitpid(worker->pid, NULL, 0);
		return;
	}
	
	worker->state = HK_WORKER_STOPPING;
	shutdown(worker->socket_in, SHUT_RDWR);
}

static void
_hk_worker_stop()
{
	hk_worker_t *worker;
	
	worker = worker_inst;
	close(worker_inst->socket_in);
	
	{
		/* Stop and close readers */
		hk_list_item_t *item;
		hk_reader_t *reader;
	
		reader = (hk_reader_t *) hk_list_pop(worker->readers);
		while ( reader != NULL ) {
			hk_reader_stop(reader);
			hk_reader_free(reader);
			reader = (hk_reader_t *) hk_list_pop(worker->readers);
		}
	}
	
	{
		/* Stop and close senders */
		hk_list_item_t *item;
		hk_sender_t *sender;
		
		sender = (hk_sender_t *) hk_list_pop(worker->senders); // worker->senders->first;
		while ( sender != NULL ) {
			hk_sender_stop(sender);
			hk_sender_free(sender);
			sender = (hk_sender_t *) hk_list_pop(worker->senders);
		}
	}
	
	worker->state = HK_WORKER_STOPPED;
}

void
hk_worker_queue_task(hk_task_t *task)
{
	hk_sender_t *sender = hk_worker_get()->active_sender;
	
	pthread_mutex_lock(sender->mu_tasks);
	hk_sender_add_task(sender, task);
	pthread_mutex_unlock(sender->mu_tasks);
	sem_post(&sender->sem_tasks);
}

void
hk_worker_on_client_connect(hk_client_t *client)
{
	client->state = HK_STATE_CONNECTING;
	hk_reader_add_client(worker_inst->active_reader, client);
}

void
hk_worker_on_client_disconnect(hk_client_t *client)
{
	if ( client == NULL ) {
		return;
	}

	// pass
}

static void 
hk_worker_signals_init()
{
    hk_signal_t      *sig;
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
hk_worker_reader_pool_init()
{
	hk_worker_t *worker = hk_worker_get();
	hk_reader_t *new_reader;
	int i;
	
	for (i = 0; i < 3; i += 1) {
		new_reader = hk_reader_init();
		hk_reader_run(new_reader);
		hk_list_push(worker->readers, new_reader);
	}
	
	worker->active_reader = (hk_reader_t *)worker->readers->first->data;
}

static void
hk_worker_sender_pool_init()
{
	hk_worker_t *worker = hk_worker_get();
	hk_sender_t *new_sender;
	int i;
	
	for (i = 0; i < 3; i += 1) {
		new_sender = hk_sender_init();
		hk_sender_run(new_sender);
		hk_list_push(worker->senders, new_sender);
	}
	
	worker->active_sender = (hk_sender_t *)worker->senders->first->data;
}

static void
hk_worker_sig(int signo)
{
	switch(signo) {
		case SIGINT:
			hk_worker_stop(worker_inst);
			break;
		default:
			break; 
	}
}


